/*
 * VLC Video Source must play the audio track selected in its settings, also
 * after the setting is changed (upstream obsproject/obs-studio#8346,
 * duplicate #5045).
 *
 * Users report that changing "Audio Track" on an existing VLC source does
 * nothing: a track that was selected before cannot be selected again, and the
 * source keeps playing the previous track.
 *
 * The test generates an AVI file with two PCM audio tracks, a 440 Hz tone on
 * track 1 and an 880 Hz tone on track 2, plays it through the real vlc_source
 * module and measures the frequency of the program mix (what outputs and
 * recordings receive) by counting zero crossings. It switches the track
 * setting 1 -> 2 -> 1 and checks that the mix follows every switch.
 *
 * The cases need core video (run under xvfb-run) and skip without a display
 * or when libVLC is not installed; the test then exits with 77 so ctest
 * reports it as skipped.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

#define RATE 48000
#define FILE_SECONDS 12
#define CHUNK_FRAMES (RATE / 10)
#define NUM_TRACKS 2

/* Tone of each audio track of the file (track 1 first). */
static const double track_hz[NUM_TRACKS] = {440.0, 880.0};

/* ------------------------------------------------------------------------ */
/* AVI file with two 16-bit stereo PCM audio tracks                          */

static void put_u16(FILE *f, uint16_t v)
{
	fwrite(&v, 2, 1, f);
}

static void put_u32(FILE *f, uint32_t v)
{
	fwrite(&v, 4, 1, f);
}

static void put_fourcc(FILE *f, const char *cc)
{
	fwrite(cc, 1, 4, f);
}

static bool write_avi(const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;

	const uint32_t block_align = 4; /* 2 channels, 16 bit */
	const uint32_t frames = RATE * FILE_SECONDS;
	const uint32_t chunks = frames / CHUNK_FRAMES;
	const uint32_t chunk_bytes = CHUNK_FRAMES * block_align;

	const uint32_t strl_size = 4 + (8 + 56) + (8 + 16);
	const uint32_t hdrl_size = 4 + (8 + 56) + NUM_TRACKS * (8 + strl_size);
	const uint32_t movi_size = 4 + chunks * NUM_TRACKS * (8 + chunk_bytes);
	const uint32_t idx1_size = chunks * NUM_TRACKS * 16;
	const uint32_t riff_size = 4 + (8 + hdrl_size) + (8 + movi_size) + (8 + idx1_size);

	put_fourcc(f, "RIFF");
	put_u32(f, riff_size);
	put_fourcc(f, "AVI ");

	put_fourcc(f, "LIST");
	put_u32(f, hdrl_size);
	put_fourcc(f, "hdrl");

	put_fourcc(f, "avih");
	put_u32(f, 56);
	put_u32(f, 100000);                          /* microseconds per "frame" (one chunk) */
	put_u32(f, RATE * block_align * NUM_TRACKS); /* max bytes per second */
	put_u32(f, 0);                               /* padding granularity */
	put_u32(f, 0x10);                            /* AVIF_HASINDEX */
	put_u32(f, chunks);                          /* total frames */
	put_u32(f, 0);                               /* initial frames */
	put_u32(f, NUM_TRACKS);                      /* streams */
	put_u32(f, chunk_bytes);                     /* suggested buffer size */
	put_u32(f, 0);                               /* width */
	put_u32(f, 0);                               /* height */
	for (int i = 0; i < 4; i++)
		put_u32(f, 0);

	for (unsigned t = 0; t < NUM_TRACKS; t++) {
		put_fourcc(f, "LIST");
		put_u32(f, strl_size);
		put_fourcc(f, "strl");

		put_fourcc(f, "strh");
		put_u32(f, 56);
		put_fourcc(f, "auds");
		put_u32(f, 0);           /* handler */
		put_u32(f, 0);           /* flags */
		put_u16(f, 0);           /* priority */
		put_u16(f, 0);           /* language */
		put_u32(f, 0);           /* initial frames */
		put_u32(f, block_align); /* scale */
		put_u32(f, RATE * block_align);
		put_u32(f, 0);      /* start */
		put_u32(f, frames); /* length in samples */
		put_u32(f, chunk_bytes);
		put_u32(f, 0xFFFFFFFF); /* quality */
		put_u32(f, block_align);
		for (int i = 0; i < 4; i++)
			put_u16(f, 0); /* rcFrame */

		put_fourcc(f, "strf");
		put_u32(f, 16);
		put_u16(f, 1); /* WAVE_FORMAT_PCM */
		put_u16(f, 2);
		put_u32(f, RATE);
		put_u32(f, RATE * block_align);
		put_u16(f, (uint16_t)block_align);
		put_u16(f, 16);
	}

	put_fourcc(f, "LIST");
	put_u32(f, movi_size);
	put_fourcc(f, "movi");

	int16_t *buf = malloc(chunk_bytes);
	for (uint32_t c = 0; c < chunks; c++) {
		for (unsigned t = 0; t < NUM_TRACKS; t++) {
			for (uint32_t i = 0; i < CHUNK_FRAMES; i++) {
				uint32_t n = c * CHUNK_FRAMES + i;
				double s = 0.5 * sin(2.0 * M_PI * track_hz[t] * (double)n / RATE);
				int16_t v = (int16_t)lrint(s * 32767.0);
				buf[i * 2] = v;
				buf[i * 2 + 1] = v;
			}
			char cc[5];
			snprintf(cc, sizeof(cc), "%02uwb", t);
			put_fourcc(f, cc);
			put_u32(f, chunk_bytes);
			fwrite(buf, 1, chunk_bytes, f);
		}
	}
	free(buf);

	/* offsets are relative to the "movi" fourcc */
	put_fourcc(f, "idx1");
	put_u32(f, idx1_size);
	uint32_t offset = 4;
	for (uint32_t c = 0; c < chunks; c++) {
		for (unsigned t = 0; t < NUM_TRACKS; t++) {
			char cc[5];
			snprintf(cc, sizeof(cc), "%02uwb", t);
			put_fourcc(f, cc);
			put_u32(f, 0x10); /* AVIIF_KEYFRAME */
			put_u32(f, offset);
			put_u32(f, chunk_bytes);
			offset += 8 + chunk_bytes;
		}
	}

	bool ok = ferror(f) == 0;
	return fclose(f) == 0 && ok;
}

/* ------------------------------------------------------------------------ */
/* Program mix frequency                                                    */

struct capture {
	pthread_mutex_t mutex;
	float prev;          /* last sample of the previous audible block */
	uint64_t frames;     /* audible frames since the last reset */
	uint64_t crossings;  /* zero crossings in them */
	uint64_t mix_frames; /* all frames since the last reset */
};

static void mix_cb(void *param, size_t mix_idx, struct audio_data *data)
{
	struct capture *cap = param;
	const float *l = (const float *)data->data[0];
	UNUSED_PARAMETER(mix_idx);

	float peak = 0.0f;
	for (uint32_t i = 0; l && i < data->frames; i++) {
		float a = fabsf(l[i]);
		if (a > peak)
			peak = a;
	}

	pthread_mutex_lock(&cap->mutex);
	cap->mix_frames += data->frames;
	if (peak > 0.1f) {
		for (uint32_t i = 0; i < data->frames; i++) {
			if ((cap->prev < 0.0f) != (l[i] < 0.0f))
				cap->crossings++;
			cap->prev = l[i];
		}
		cap->frames += data->frames;
	} else {
		cap->prev = 0.0f;
	}
	pthread_mutex_unlock(&cap->mutex);
}

/* Frequency of the program mix over the next `ms` milliseconds; 0 when it
 * was silent for most of that time. */
static double measure_hz(struct capture *cap, unsigned ms)
{
	pthread_mutex_lock(&cap->mutex);
	cap->frames = 0;
	cap->crossings = 0;
	cap->mix_frames = 0;
	pthread_mutex_unlock(&cap->mutex);

	os_sleep_ms(ms);

	pthread_mutex_lock(&cap->mutex);
	uint64_t frames = cap->frames;
	uint64_t crossings = cap->crossings;
	uint64_t mix_frames = cap->mix_frames;
	pthread_mutex_unlock(&cap->mutex);

	if (!mix_frames || frames < mix_frames / 2)
		return 0.0;
	return (double)crossings / 2.0 / ((double)frames / RATE);
}

/* Track (1-based) whose tone the mix carries, 0 if none. */
static int track_of(double hz)
{
	for (int t = 0; t < NUM_TRACKS; t++) {
		if (fabs(hz - track_hz[t]) < track_hz[t] * 0.1)
			return t + 1;
	}
	return 0;
}

/* ------------------------------------------------------------------------ */
/* libobs setup                                                             */

/* libobs is up (a display is available) */
static bool obs_up;
/* ... and the module registered vlc_source (libVLC is installed) */
static bool vlc_ready;

static int start_obs(void **state)
{
	UNUSED_PARAMETER(state);
	const char *display = getenv("DISPLAY");
	if (!display || !*display)
		return 0;

	if (!obs_startup("en-US", NULL, NULL))
		return 0;

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = 64,
		.base_height = 64,
		.output_width = 64,
		.output_height = 64,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return 0;

	struct obs_audio_info oai = {.samples_per_sec = RATE, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai))
		return 0;

	obs_up = true;

	obs_module_t *mod = NULL;
	if (obs_open_module(&mod, VLC_MODULE_PATH, VLC_DATA_PATH) != MODULE_SUCCESS || !obs_init_module(mod))
		return 0;

	/* the module only registers vlc_source when libVLC is installed */
	vlc_ready = obs_source_get_display_name("vlc_source") != NULL;
	return 0;
}

static int stop_obs(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_initialized())
		obs_shutdown();
	return 0;
}

/* ------------------------------------------------------------------------ */
/* Playing the file                                                          */

/* After a settings change vlc_source restarts the file; give libVLC and the
 * libobs audio buffering time to settle before measuring. */
#define SETTLE_MS 1500
#define MEASURE_MS 1000

/* Plays the file with the audio track set to tracks[0], then changes the
 * setting to each of the following tracks in turn. Returns true when the mix
 * carried the selected track after every step. */
static bool play_tracks(const int *tracks, size_t num)
{
	char path[512];
	snprintf(path, sizeof(path), "%s/obs-vlc-track-%llu.avi", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp",
		 (unsigned long long)os_gettime_ns());
	if (!write_avi(path)) {
		fprintf(stderr, "could not write %s\n", path);
		fail();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_array_t *list = obs_data_array_create();
	obs_data_t *item = obs_data_create();
	obs_data_set_string(item, "value", path);
	obs_data_array_push_back(list, item);
	obs_data_release(item);
	obs_data_set_array(settings, "playlist", list);
	obs_data_array_release(list);
	obs_data_set_string(settings, "playback_behavior", "always_play");
	obs_data_set_bool(settings, "loop", false);
	obs_data_set_int(settings, "track", tracks[0]);

	struct capture cap;
	memset(&cap, 0, sizeof(cap));
	pthread_mutex_init(&cap.mutex, NULL);
	obs_add_raw_audio_callback(0, NULL, mix_cb, &cap);

	obs_source_t *src = obs_source_create("vlc_source", "vlc", settings, NULL);
	if (!src) {
		obs_data_release(settings);
		obs_remove_raw_audio_callback(0, mix_cb, &cap);
		pthread_mutex_destroy(&cap.mutex);
		os_unlink(path);
		fprintf(stderr, "could not create vlc_source\n");
		fail();
	}
	obs_set_output_source(0, src);

	bool ok = true;
	for (size_t i = 0; i < num; i++) {
		if (i > 0) {
			obs_data_set_int(settings, "track", tracks[i]);
			obs_source_update(src, settings);
		}
		os_sleep_ms(SETTLE_MS);
		double hz = measure_hz(&cap, MEASURE_MS);
		int got = track_of(hz);
		fprintf(stderr, "step %zu: track setting %d, program mix %.1f Hz (track %d)\n", i + 1, tracks[i], hz,
			got);
		if (got != tracks[i]) {
			fprintf(stderr, "step %zu: the mix carries track %d, the setting is track %d\n", i + 1, got,
				tracks[i]);
			ok = false;
		}
	}

	obs_set_output_source(0, NULL);
	obs_remove_raw_audio_callback(0, mix_cb, &cap);
	obs_source_release(src);
	obs_data_release(settings);
	pthread_mutex_destroy(&cap.mutex);
	os_unlink(path);
	return ok;
}

/* ------------------------------------------------------------------------ */
/* Cases                                                                    */

/* Guard: the track selected when the source is created plays. */
static void initial_track_plays(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip(); /* no display */
	if (!vlc_ready)
		skip(); /* libVLC is not installed */

	static const int first[] = {1};
	static const int second[] = {2};
	bool ok = play_tracks(first, 1) & play_tracks(second, 1);
	if (!ok) {
		fprintf(stderr, "the initially selected audio track does not play\n");
		fail();
	}
}

/* The reported bug: switch to track 2 and back to track 1. */
static void switching_track_follows_setting(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	if (!vlc_ready)
		skip();

	static const int tracks[] = {1, 2, 1};
	if (!play_tracks(tracks, sizeof(tracks) / sizeof(tracks[0]))) {
		fprintf(stderr, "changing the audio track did not take effect\n");
		fail();
	}
}

/* Starting on track 2: back to 1, then to 2 again. */
static void switching_back_to_initial_track(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	if (!vlc_ready)
		skip();

	static const int tracks[] = {2, 1, 2};
	if (!play_tracks(tracks, sizeof(tracks) / sizeof(tracks[0]))) {
		fprintf(stderr, "changing the audio track did not take effect\n");
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(initial_track_plays),
		cmocka_unit_test(switching_track_follows_setting),
		cmocka_unit_test(switching_back_to_initial_track),
	};

	int failed = cmocka_run_group_tests(tests, start_obs, stop_obs);
	if (failed)
		return failed;
	return vlc_ready ? 0 : 77;
}
