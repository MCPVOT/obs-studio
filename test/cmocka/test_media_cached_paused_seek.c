/*
 * A seek on a cached local video publishes its selected frame through the
 * video seek callback only while paused, as the non-cached path does
 * (media.c: `m->seek_next_ts && m->pause && m->v_preload_cb`). A seek during
 * playback must not call the seek callback; the normal output path shows the
 * landed frame.
 *
 * Fixture data/media_cached_paused_seek.mkv (5 s: 16x16 ffv1 at 10 fps whose
 * luma is 5 * frame_index, plus 48 kHz mono s16 audio whose sample value is
 * 500 * (100 ms interval + 1)) was generated with:
 *
 *   ffmpeg -f lavfi -i "color=c=black:s=16x16:r=10:d=5,format=yuv420p,geq=lum='5*N':cb=128:cr=128" \
 *          -f lavfi -i "aevalsrc='500*(floor(t*10)+1)/32768':s=48000:d=5:c=mono" \
 *          -c:v ffv1 -c:a pcm_s16le -t 5 media_cached_paused_seek.mkv
 *
 * (The same decoded content is committed under other names by sibling media
 * test branches; only the container bytes differ between ffmpeg runs.)
 *
 * Every request is issued from the cache thread's own video callback, after
 * a real frame, so no sleeps or libobs test hooks are needed for ordering.
 * Negative results are proven by event barriers, not by waiting:
 *  - playing: the cache thread is sequential, so a normal frame delivered after
 *    the seek request proves the seek was already handled;
 *  - paused: the test waits until the cache thread has taken the seek request
 *    (cache.seek cleared under cache.mutex), then requests a stop. The stop is
 *    handled in a later loop iteration than the seek, so stop_cb runs after any
 *    seek callback.
 */

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sched.h>

#include <cmocka.h>

#include <util/c99defs.h>
#include <media-playback/media-playback.h>
#include <media-playback/cache.h>

/* Liveness bound only; no result depends on elapsed time. */
#define WAIT_SECONDS 10
/* Normal-output frame (0-based) whose callback issues the request. Frame 2 has luma 10. */
#define TRIGGER_FRAME 2
#define PRE_SEEK_LUMA 10
/* Normal-output frames recorded, and how many must have arrived (the trigger plus two) in the playing case. */
#define MAX_NORMAL 8
#define PLAYING_FRAMES_NEEDED (TRIGGER_FRAME + 3)
/* 50 ms lies between frame 0 (0 ms) and frame 1 (100 ms), so the first frame at or after it is frame 1 (luma 5)
 * whether or not cached seek units are corrected. */
#define NONZERO_SEEK_MS 50
#define NONZERO_SEEK_LUMA 5
#define ZERO_SEEK_LUMA 0

struct seek_watch {
	mp_cache_t *cache;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool pause_on_trigger;
	int64_t seek_ms;
	unsigned normal_callbacks;
	uint8_t normal_luma[MAX_NORMAL];
	unsigned seek_callbacks;
	uint8_t seek_luma;
	unsigned stop_callbacks;
	bool seek_issued;
};

static const char *fixture_path;
static mp_cache_t cache;
static struct seek_watch watch;
static bool cache_ready;
static bool watch_ready;

static void video_callback(void *opaque, struct obs_source_frame *frame)
{
	struct seek_watch *state = opaque;
	uint8_t luma = frame->data[0] ? frame->data[0][0] : 0;

	pthread_mutex_lock(&state->mutex);
	unsigned callback = state->normal_callbacks++;
	if (callback < MAX_NORMAL)
		state->normal_luma[callback] = luma;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);

	if (callback != TRIGGER_FRAME)
		return;

	if (state->pause_on_trigger)
		mp_cache_play_pause(state->cache, true);
	mp_cache_seek(state->cache, state->seek_ms);

	pthread_mutex_lock(&state->mutex);
	state->seek_issued = true;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

static void video_seek_callback(void *opaque, struct obs_source_frame *frame)
{
	struct seek_watch *state = opaque;
	uint8_t luma = frame->data[0] ? frame->data[0][0] : 0;

	pthread_mutex_lock(&state->mutex);
	state->seek_luma = luma;
	state->seek_callbacks++;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

static void stop_callback(void *opaque)
{
	struct seek_watch *state = opaque;

	pthread_mutex_lock(&state->mutex);
	state->stop_callbacks++;
	pthread_cond_broadcast(&state->cond);
	pthread_mutex_unlock(&state->mutex);
}

static int start_cache(bool pause_on_trigger, int64_t seek_ms)
{
	memset(&cache, 0, sizeof(cache));
	memset(&watch, 0, sizeof(watch));
	watch.cache = &cache;
	watch.pause_on_trigger = pause_on_trigger;
	watch.seek_ms = seek_ms;
	if (pthread_mutex_init(&watch.mutex, NULL) != 0)
		return -1;
	if (pthread_cond_init(&watch.cond, NULL) != 0) {
		pthread_mutex_destroy(&watch.mutex);
		return -1;
	}
	watch_ready = true;

	struct mp_media_info info = {
		.opaque = &watch,
		.v_cb = video_callback,
		.v_seek_cb = video_seek_callback,
		.stop_cb = stop_callback,
		.path = fixture_path,
		.speed = 100,
		.is_local_file = true,
		.full_decode = true,
	};
	cache_ready = mp_cache_init(&cache, &info);
	return cache_ready ? 0 : -1;
}

static int paused_zero_setup(void **state)
{
	UNUSED_PARAMETER(state);
	return start_cache(true, 0);
}

static int paused_nonzero_setup(void **state)
{
	UNUSED_PARAMETER(state);
	return start_cache(true, NONZERO_SEEK_MS);
}

static int playing_setup(void **state)
{
	UNUSED_PARAMETER(state);
	return start_cache(false, NONZERO_SEEK_MS);
}

static int test_teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (cache_ready) {
		mp_cache_free(&cache);
		cache_ready = false;
	}
	if (watch_ready) {
		pthread_cond_destroy(&watch.cond);
		pthread_mutex_destroy(&watch.mutex);
		watch_ready = false;
	}
	return 0;
}

static struct timespec make_deadline(void)
{
	struct timespec deadline;
	assert_int_equal(timespec_get(&deadline, TIME_UTC), TIME_UTC);
	deadline.tv_sec += WAIT_SECONDS;
	return deadline;
}

static bool past_deadline(const struct timespec *deadline)
{
	struct timespec now;
	if (timespec_get(&now, TIME_UTC) != TIME_UTC)
		return true;
	return now.tv_sec > deadline->tv_sec || (now.tv_sec == deadline->tv_sec && now.tv_nsec >= deadline->tv_nsec);
}

/* Paused: wait for the request, wait until the cache thread has taken it, then stop
 * and wait for stop_cb. Any seek callback has run by then. */
static void run_paused_and_stop(void)
{
	mp_cache_play(&cache, false);

	struct timespec deadline = make_deadline();
	pthread_mutex_lock(&watch.mutex);
	int wait_result = 0;
	while (!watch.seek_issued && wait_result == 0)
		wait_result = pthread_cond_timedwait(&watch.cond, &watch.mutex, &deadline);
	pthread_mutex_unlock(&watch.mutex);
	assert_int_equal(wait_result, 0);

	/* cache.seek is cleared under cache.mutex when the cache thread takes the request. */
	bool pending = true;
	while (pending) {
		pthread_mutex_lock(&cache.mutex);
		pending = cache.seek;
		pthread_mutex_unlock(&cache.mutex);
		if (pending) {
			assert_false(past_deadline(&deadline));
			sched_yield();
		}
	}

	mp_cache_stop(&cache);

	pthread_mutex_lock(&watch.mutex);
	while (!watch.stop_callbacks && wait_result == 0)
		wait_result = pthread_cond_timedwait(&watch.cond, &watch.mutex, &deadline);
	pthread_mutex_unlock(&watch.mutex);
	assert_int_equal(wait_result, 0);
}

static void check_paused_seek(uint8_t expected_luma)
{
	run_paused_and_stop();

	pthread_mutex_lock(&watch.mutex);
	unsigned normal_callbacks = watch.normal_callbacks;
	unsigned seek_callbacks = watch.seek_callbacks;
	uint8_t seek_luma = watch.seek_luma;
	uint8_t trigger_luma = watch.normal_luma[TRIGGER_FRAME];
	pthread_mutex_unlock(&watch.mutex);

	if (seek_callbacks != 1 || seek_luma != expected_luma)
		fprintf(stderr, "paused seek: normal=%u seek_callbacks=%u seek_luma=%u expected_luma=%u\n",
			normal_callbacks, seek_callbacks, seek_luma, expected_luma);
	/* The request came from the third normal frame, whose luma differs from every expected landing. */
	assert_int_equal(trigger_luma, PRE_SEEK_LUMA);
	assert_int_equal(seek_callbacks, 1);
	assert_int_equal(seek_luma, expected_luma);
}

static void paused_seek_to_start_publishes_first_frame(void **state)
{
	UNUSED_PARAMETER(state);
	check_paused_seek(ZERO_SEEK_LUMA);
}

static void paused_seek_to_nonzero_publishes_selected_frame(void **state)
{
	UNUSED_PARAMETER(state);
	check_paused_seek(NONZERO_SEEK_LUMA);
}

static void playing_seek_does_not_publish_seek_frame(void **state)
{
	UNUSED_PARAMETER(state);
	mp_cache_play(&cache, false);

	struct timespec deadline = make_deadline();
	pthread_mutex_lock(&watch.mutex);
	int wait_result = 0;
	while (watch.normal_callbacks < PLAYING_FRAMES_NEEDED && wait_result == 0)
		wait_result = pthread_cond_timedwait(&watch.cond, &watch.mutex, &deadline);
	unsigned normal_callbacks = watch.normal_callbacks;
	unsigned seek_callbacks = watch.seek_callbacks;
	uint8_t trigger_luma = watch.normal_luma[TRIGGER_FRAME];
	uint8_t landed_luma = watch.normal_luma[TRIGGER_FRAME + 1];
	pthread_mutex_unlock(&watch.mutex);

	if (wait_result != 0 || seek_callbacks != 0 || landed_luma != NONZERO_SEEK_LUMA)
		fprintf(stderr, "playing seek: normal=%u seek_callbacks=%u landed_luma=%u wait=%d\n", normal_callbacks,
			seek_callbacks, landed_luma, wait_result);
	assert_int_equal(wait_result, 0);
	assert_int_equal(trigger_luma, PRE_SEEK_LUMA);
	/* The first frame after the request is the landed frame, so the seek was handled before the check below. */
	assert_int_equal(landed_luma, NONZERO_SEEK_LUMA);
	assert_int_equal(seek_callbacks, 0);
}

int main(int argc, char **argv)
{
	if (argc != 2)
		return 2;
	fixture_path = argv[1];
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(paused_seek_to_start_publishes_first_frame, paused_zero_setup,
						test_teardown),
		cmocka_unit_test_setup_teardown(paused_seek_to_nonzero_publishes_selected_frame, paused_nonzero_setup,
						test_teardown),
		cmocka_unit_test_setup_teardown(playing_seek_does_not_publish_seek_frame, playing_setup, test_teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
