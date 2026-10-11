/*
 * A Media Source with "Restart playback when source becomes active" must not
 * show the last frame of its previous playback when it becomes active again
 * (upstream obsproject/obs-studio#4667, #2906).
 *
 * With "Show nothing when playback ends" off, the last frame stays on the
 * source after playback ends, which is intended. When the source leaves the
 * program and comes back, activation only queues the restart; libobs runs
 * queued media actions at the start of the next video tick, so the tick that
 * activates the source still draws the old last frame before playback
 * starts over.
 *
 * The test plays a short generated clip (bright first frame, dark last frame)
 * through the real ffmpeg_source in a scene, lets it end, switches the
 * program to an empty scene and back, and watches every frame libobs outputs:
 * the dark last frame must not appear before the bright first frame.
 * It needs core video, so it runs under xvfb-run and skips without a
 * display.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <obs-module.h>
#include <util/platform.h>
#include <util/threading.h>

/* obs-ffmpeg-source.c is compiled into this test; it only needs strings. */
const char *obs_module_text(const char *val)
{
	return val;
}

extern struct obs_source_info ffmpeg_source;

#define W 64
#define H 64
#define CLIP_FRAMES 6

/* Luma written into each clip frame. */
#define Y_FIRST 220
#define Y_MIDDLE 128
#define Y_LAST 70

static bool write_clip(const char *path)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return false;

	fprintf(f, "YUV4MPEG2 W%d H%d F10:1 Ip A1:1 C420jpeg\n", W, H);

	static uint8_t y[W * H], uv[(W / 2) * (H / 2)];
	memset(uv, 128, sizeof(uv));
	for (int i = 0; i < CLIP_FRAMES; i++) {
		int luma = i == 0 ? Y_FIRST : (i == CLIP_FRAMES - 1 ? Y_LAST : Y_MIDDLE);
		memset(y, luma, sizeof(y));
		fputs("FRAME\n", f);
		fwrite(y, 1, sizeof(y), f);
		fwrite(uv, 1, sizeof(uv), f);
		fwrite(uv, 1, sizeof(uv), f);
	}

	fclose(f);
	return true;
}

enum shown { SHOWN_NOTHING, SHOWN_FIRST, SHOWN_MIDDLE, SHOWN_LAST };

static const char *shown_name(enum shown s)
{
	switch (s) {
	case SHOWN_NOTHING:
		return "nothing";
	case SHOWN_FIRST:
		return "first frame";
	case SHOWN_MIDDLE:
		return "middle frame";
	case SHOWN_LAST:
		return "LAST frame";
	}
	return "?";
}

/* Output is NV12, partial range: classify the luma at the centre. */
static enum shown classify(uint8_t luma)
{
	if (luma < 40)
		return SHOWN_NOTHING;
	if (luma < 100)
		return SHOWN_LAST;
	if (luma < 170)
		return SHOWN_MIDDLE;
	return SHOWN_FIRST;
}

#define MAX_SEEN 512

struct watch {
	pthread_mutex_t mutex;
	enum shown last;
	bool recording;
	size_t count;
	enum shown seen[MAX_SEEN];
};

static void raw_video(void *param, struct video_data *frame)
{
	struct watch *w = param;
	enum shown s = classify(frame->data[0][(H / 2) * frame->linesize[0] + W / 2]);

	pthread_mutex_lock(&w->mutex);
	w->last = s;
	if (w->recording && w->count < MAX_SEEN)
		w->seen[w->count++] = s;
	pthread_mutex_unlock(&w->mutex);
}

static enum shown last_shown(struct watch *w)
{
	pthread_mutex_lock(&w->mutex);
	enum shown s = w->last;
	pthread_mutex_unlock(&w->mutex);
	return s;
}

static bool init_core(void)
{
	if (!obs_startup("en-US", NULL, NULL))
		return false;

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 30,
		.fps_den = 1,
		.base_width = W,
		.base_height = H,
		.output_width = W,
		.output_height = H,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return false;

	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai))
		return false;

	obs_register_source(&ffmpeg_source);
	return true;
}

static bool wait_for(struct watch *w, enum shown want, int ms)
{
	for (int i = 0; i < ms / 10; i++) {
		if (last_shown(w) == want)
			return true;
		os_sleep_ms(10);
	}
	return false;
}

static void restart_does_not_show_last_frame(void **state)
{
	UNUSED_PARAMETER(state);

	const char *display = getenv("DISPLAY");
	if (!display || !*display)
		skip();

	if (!init_core()) {
		obs_shutdown();
		skip();
	}

	char path[512];
	snprintf(path, sizeof(path), "%s/obs-media-restart-%llu.y4m", getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp",
		 (unsigned long long)os_gettime_ns());
	if (!write_clip(path)) {
		fprintf(stderr, "could not write the test clip %s\n", path);
		fail();
	}

	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", path);
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "clear_on_media_end", false);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "hw_decode", false);
	obs_source_t *media = obs_source_create("ffmpeg_source", "media", settings, NULL);
	obs_data_release(settings);
	if (!media) {
		fprintf(stderr, "could not create the media source\n");
		fail();
	}

	obs_scene_t *with_media = obs_scene_create("with media");
	obs_scene_t *empty = obs_scene_create("empty");
	obs_scene_add(with_media, media);

	struct watch w = {.last = SHOWN_NOTHING};
	pthread_mutex_init(&w.mutex, NULL);
	obs_add_raw_video_callback(NULL, raw_video, &w);

	/* First playback: runs to the end and keeps showing its last frame. */
	obs_set_output_source(0, obs_scene_get_source(with_media));
	bool ended = false;
	for (int i = 0; i < 500 && !ended; i++) {
		os_sleep_ms(10);
		ended = obs_source_media_get_state(media) == OBS_MEDIA_STATE_ENDED;
	}
	bool holds_last = ended && wait_for(&w, SHOWN_LAST, 2000);

	/* Away to an empty scene, then back. */
	obs_set_output_source(0, obs_scene_get_source(empty));
	bool cleared = wait_for(&w, SHOWN_NOTHING, 2000);
	os_sleep_ms(300);

	pthread_mutex_lock(&w.mutex);
	w.recording = true;
	pthread_mutex_unlock(&w.mutex);

	obs_set_output_source(0, obs_scene_get_source(with_media));
	os_sleep_ms(1500);

	pthread_mutex_lock(&w.mutex);
	w.recording = false;
	pthread_mutex_unlock(&w.mutex);

	obs_remove_raw_video_callback(raw_video, &w);
	obs_set_output_source(0, NULL);
	/* the main canvas holds a reference to its scenes until they are removed */
	obs_source_remove(obs_scene_get_source(with_media));
	obs_source_remove(obs_scene_get_source(empty));
	obs_scene_release(with_media);
	obs_scene_release(empty);
	obs_source_release(media);
	obs_shutdown();
	os_unlink(path);
	pthread_mutex_destroy(&w.mutex);

	if (!ended || !holds_last || !cleared) {
		fprintf(stderr, "setup failed: playback ended %d, last frame held %d, empty scene shown %d\n", ended,
			holds_last, cleared);
		fail();
	}

	/* Collapse runs for a readable log. */
	fprintf(stderr, "output after switching back:");
	for (size_t i = 0; i < w.count; i++) {
		if (i == 0 || w.seen[i] != w.seen[i - 1])
			fprintf(stderr, " %s", shown_name(w.seen[i]));
	}
	fprintf(stderr, "\n");

	/* Under load the player may drop the first frame, so any frame before
	 * the last one shows that playback restarted. */
	bool restarted = false;
	size_t stale = 0;
	for (size_t i = 0; i < w.count; i++) {
		if (w.seen[i] == SHOWN_FIRST || w.seen[i] == SHOWN_MIDDLE)
			restarted = true;
		else if (w.seen[i] == SHOWN_LAST && !restarted)
			stale++;
	}

	if (!restarted) {
		fprintf(stderr, "playback did not restart\n");
		fail();
	}
	if (stale) {
		fprintf(stderr, "%zu output frame(s) showed the previous playback's last frame before the restart\n",
			stale);
		fail();
	}
}

/* ------------------------------------------------------------------------ */
/* The libobs half on its own: obs_source_output_video(source, NULL) clears an
 * async source. A frame queued after the clear only reaches the textures when
 * it is rendered, so until then libobs must not draw the picture from before
 * the clear, and once it is rendered it must show at once. A small scripted
 * async source does each step from its video_tick, which runs on the graphics
 * thread after the async tick and before the render, so every step lands on
 * a known tick. */

enum script {
	SCRIPT_NONE,
	/* tick 0: clear; tick 1: queue a new frame (the Media Source restart) */
	SCRIPT_CLEAR_THEN_FRAME,
	/* tick 0: clear and nothing more */
	SCRIPT_CLEAR,
	/* tick 0: clear; tick 1: show a preloaded frame (Media Source with
	 * looping or "show nothing when playback ends") */
	SCRIPT_CLEAR_THEN_PRELOADED,
};

struct scripted {
	obs_source_t *source;
	volatile long script;
	volatile long step;
};

static void fill_frame(struct obs_source_frame *frame, uint8_t *data, uint8_t gray)
{
	memset(data, gray, W * H * 4);
	for (int i = 0; i < W * H; i++)
		data[i * 4 + 3] = 255;

	*frame = (struct obs_source_frame){
		.data = {data},
		.linesize = {W * 4},
		.width = W,
		.height = H,
		.format = VIDEO_FORMAT_BGRA,
		.full_range = true,
		.timestamp = os_gettime_ns(),
	};
}

static const char *scripted_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "scripted async source";
}

static void *scripted_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	struct scripted *s = bzalloc(sizeof(*s));
	s->source = source;
	return s;
}

static void scripted_destroy(void *data)
{
	bfree(data);
}

static void scripted_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);
	struct scripted *s = data;
	long script = os_atomic_load_long(&s->script);
	if (script == SCRIPT_NONE)
		return;

	static uint8_t pixels[W * H * 4];
	struct obs_source_frame frame;
	long step = s->step++;

	if (step == 0) {
		obs_source_output_video(s->source, NULL);
		if (script == SCRIPT_CLEAR)
			os_atomic_set_long(&s->script, SCRIPT_NONE);
		return;
	}

	fill_frame(&frame, pixels, Y_FIRST);
	if (script == SCRIPT_CLEAR_THEN_FRAME) {
		obs_source_output_video(s->source, &frame);
	} else {
		obs_source_preload_video(s->source, &frame);
		obs_source_show_preloaded_video(s->source);
	}
	os_atomic_set_long(&s->script, SCRIPT_NONE);
}

static struct obs_source_info scripted_source = {
	.id = "scripted_async_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = scripted_name,
	.create = scripted_create,
	.destroy = scripted_destroy,
	.video_tick = scripted_tick,
};

enum variant { PLAIN, UNBUFFERED };

static const char *variant_name(enum variant v)
{
	switch (v) {
	case PLAIN:
		return "buffered";
	case UNBUFFERED:
		return "unbuffered";
	}
	return "?";
}

/* Runs a script on a source that shows the old (dark) picture, records the
 * output frames from the moment the script starts and counts the frames the
 * video thread lagged meanwhile. Skips without core video. */
static void run_script(enum script script, enum variant variant, struct watch *w, uint32_t *lagged)
{
	const char *display = getenv("DISPLAY");
	if (!display || !*display)
		skip();

	if (!init_core()) {
		obs_shutdown();
		skip();
	}
	obs_register_source(&scripted_source);

	obs_source_t *source = obs_source_create("scripted_async_source", "scripted", NULL, NULL);
	if (variant == UNBUFFERED)
		obs_source_set_async_unbuffered(source, true);
	struct scripted *s = obs_obj_get_data(source);

	obs_scene_t *scene = obs_scene_create("scene");
	obs_scene_add(scene, source);

	pthread_mutex_init(&w->mutex, NULL);
	w->last = SHOWN_NOTHING;
	obs_add_raw_video_callback(NULL, raw_video, w);
	obs_set_output_source(0, obs_scene_get_source(scene));

	static uint8_t pixels[W * H * 4];
	struct obs_source_frame frame;
	fill_frame(&frame, pixels, Y_LAST);
	obs_source_output_video(source, &frame);
	bool shown = wait_for(w, SHOWN_LAST, 2000);
	os_sleep_ms(200);

	pthread_mutex_lock(&w->mutex);
	w->recording = true;
	pthread_mutex_unlock(&w->mutex);

	uint32_t lagged_before = obs_get_lagged_frames();
	os_atomic_set_long(&s->script, script);
	os_sleep_ms(800);

	pthread_mutex_lock(&w->mutex);
	w->recording = false;
	pthread_mutex_unlock(&w->mutex);
	*lagged = obs_get_lagged_frames() - lagged_before;

	obs_remove_raw_video_callback(raw_video, w);
	obs_set_output_source(0, NULL);
	obs_source_remove(obs_scene_get_source(scene));
	obs_scene_release(scene);
	obs_source_release(source);
	obs_shutdown();
	pthread_mutex_destroy(&w->mutex);

	if (!shown) {
		fprintf(stderr, "%s: the old picture was never shown\n", variant_name(variant));
		fail();
	}
}

static void log_seen(const char *what, enum variant variant, const struct watch *w)
{
	fprintf(stderr, "%s (%s):", what, variant_name(variant));
	for (size_t i = 0; i < w->count; i++) {
		if (i == 0 || w->seen[i] != w->seen[i - 1])
			fprintf(stderr, " %s", shown_name(w->seen[i]));
	}
	fprintf(stderr, "\n");
}

/* Recorded frames from the first "nothing" on: where the clear took effect. */
static size_t first_cleared(const struct watch *w)
{
	for (size_t i = 0; i < w->count; i++) {
		if (w->seen[i] == SHOWN_NOTHING)
			return i;
	}
	return w->count;
}

static void clear_then_frame(enum variant variant)
{
	static struct watch w;
	memset(&w, 0, sizeof(w));
	uint32_t lagged;
	run_script(SCRIPT_CLEAR_THEN_FRAME, variant, &w, &lagged);
	log_seen("clear, then a new frame on the next tick", variant, &w);

	size_t cleared = first_cleared(&w);
	if (cleared == w.count) {
		fprintf(stderr, "the clear never took effect\n");
		fail();
	}
	size_t stale = 0, blank = 0;
	bool saw_new = false;
	for (size_t i = cleared; i < w.count; i++) {
		if (w.seen[i] == SHOWN_FIRST)
			saw_new = true;
		else if (w.seen[i] == SHOWN_LAST)
			stale++;
		else if (!saw_new)
			blank++;
	}
	if (!saw_new) {
		fprintf(stderr, "the new frame was never shown\n");
		fail();
	}
	if (stale) {
		fprintf(stderr, "%zu output frame(s) showed the cleared picture again\n", stale);
		fail();
	}
	/* Cleared on one tick, queued on the next, drawn on the one after:
	 * two blank ticks, plus any the video thread lagged. */
	if (blank > 2 + lagged) {
		fprintf(stderr, "the new frame was held back for %zu output frames\n", blank);
		fail();
	}
}

static void clear_then_frame_buffered(void **state)
{
	UNUSED_PARAMETER(state);
	clear_then_frame(PLAIN);
}

static void clear_then_frame_unbuffered(void **state)
{
	UNUSED_PARAMETER(state);
	clear_then_frame(UNBUFFERED);
}

static void clear_still_clears(void **state)
{
	UNUSED_PARAMETER(state);
	static struct watch w;
	memset(&w, 0, sizeof(w));
	uint32_t lagged;
	run_script(SCRIPT_CLEAR, PLAIN, &w, &lagged);
	log_seen("clear", PLAIN, &w);

	size_t cleared = first_cleared(&w);
	if (cleared == w.count) {
		fprintf(stderr, "the clear never took effect\n");
		fail();
	}
	for (size_t i = cleared; i < w.count; i++) {
		if (w.seen[i] != SHOWN_NOTHING) {
			fprintf(stderr, "a cleared source drew %s\n", shown_name(w.seen[i]));
			fail();
		}
	}
}

static void clear_then_preloaded_frame(void **state)
{
	UNUSED_PARAMETER(state);
	static struct watch w;
	memset(&w, 0, sizeof(w));
	uint32_t lagged;
	run_script(SCRIPT_CLEAR_THEN_PRELOADED, PLAIN, &w, &lagged);
	log_seen("clear, then show a preloaded frame on the next tick", PLAIN, &w);

	size_t cleared = first_cleared(&w);
	size_t blank = 0;
	bool saw_new = false;
	for (size_t i = cleared; i < w.count; i++) {
		if (w.seen[i] == SHOWN_FIRST)
			saw_new = true;
		else if (w.seen[i] == SHOWN_LAST) {
			fprintf(stderr, "a cleared source drew the cleared picture again\n");
			fail();
		} else if (!saw_new)
			blank++;
	}
	if (!saw_new) {
		fprintf(stderr, "the preloaded frame was never shown\n");
		fail();
	}
	/* shown on the tick right after the clear */
	if (blank > 1 + lagged) {
		fprintf(stderr, "the preloaded frame was held back for %zu output frames\n", blank);
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(restart_does_not_show_last_frame), cmocka_unit_test(clear_then_frame_buffered),
		cmocka_unit_test(clear_then_frame_unbuffered),      cmocka_unit_test(clear_still_clears),
		cmocka_unit_test(clear_then_preloaded_frame),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
