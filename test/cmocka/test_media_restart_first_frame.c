/*
 * A Media Source with "Restart playback when source becomes active" must show
 * the first frame of the restarted playback on the tick it becomes active,
 * not one empty (transparent) frame first (upstream obsproject/obs-studio#10488).
 *
 * Activation only queues the restart, and libobs runs queued media actions
 * on the next video tick. To keep the previous playback's frame from flashing
 * on the activating tick (#4667), the source is cleared there, so that tick
 * shows nothing: the scene behind it shows through for one frame.
 *
 * The cases play a short generated clip (bright first frame, dark last frame)
 * through the real ffmpeg_source in a scene with a red background behind it,
 * switch the program to an empty (black) scene and back, and classify every
 * output frame from the activating tick (the first frame that is not black)
 * on. Before the restarted playback moves past its first frame, no frame may
 * show the red background (the source drawing nothing) or a frame of the
 * previous playback. "Show nothing when playback ends" is checked on and off,
 * and with the switch made after playback ended and in the middle of it.
 * They need core video, so they run under xvfb-run and skip without a
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

/* Luma written into each clip frame; the chroma is neutral. */
#define Y_FIRST 220
#define Y_MIDDLE 128
#define Y_LAST 70

static bool obs_up;
static char clip_path[512];

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

/* ------------------------------------------------------------------------ */
/* A red background drawn behind the media source, so a source that draws
 * nothing is told apart from the empty scene (black). */

static const char *background_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "red background";
}

static void *background_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return source;
}

static void background_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static uint32_t background_width(void *data)
{
	UNUSED_PARAMETER(data);
	return W;
}

static uint32_t background_height(void *data)
{
	UNUSED_PARAMETER(data);
	return H;
}

static void background_render(void *data, gs_effect_t *unused)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(unused);

	gs_effect_t *solid = obs_get_base_effect(OBS_EFFECT_SOLID);
	struct vec4 red;
	vec4_set(&red, 1.0f, 0.0f, 0.0f, 1.0f);
	gs_effect_set_vec4(gs_effect_get_param_by_name(solid, "color"), &red);
	gs_technique_t *tech = gs_effect_get_technique(solid, "Solid");
	gs_technique_begin(tech);
	gs_technique_begin_pass(tech, 0);
	gs_draw_sprite(NULL, 0, W, H);
	gs_technique_end_pass(tech);
	gs_technique_end(tech);
}

static struct obs_source_info background_source = {
	.id = "test_red_background",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW,
	.get_name = background_name,
	.create = background_create,
	.destroy = background_destroy,
	.get_width = background_width,
	.get_height = background_height,
	.video_render = background_render,
};

/* ------------------------------------------------------------------------ */

enum shown { SHOWN_BLACK, SHOWN_EMPTY, SHOWN_FIRST, SHOWN_MIDDLE, SHOWN_LAST };

static const char *shown_name(enum shown s)
{
	switch (s) {
	case SHOWN_BLACK:
		return "empty scene";
	case SHOWN_EMPTY:
		return "NOTHING (background)";
	case SHOWN_FIRST:
		return "first frame";
	case SHOWN_MIDDLE:
		return "middle frame";
	case SHOWN_LAST:
		return "LAST frame";
	}
	return "?";
}

/* Output is BGRA: classify the centre pixel. The clip is grey, the
 * background red and the empty scene black. */
static enum shown classify(const uint8_t *bgra)
{
	uint8_t b = bgra[0], g = bgra[1], r = bgra[2];
	if (r > 150 && g < 60 && b < 60)
		return SHOWN_EMPTY;
	if (g < 40)
		return SHOWN_BLACK;
	if (g < 100)
		return SHOWN_LAST;
	if (g < 170)
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
	enum shown s = classify(frame->data[0] + (H / 2) * frame->linesize[0] + (W / 2) * 4);
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

static bool wait_for(struct watch *w, enum shown want, int ms)
{
	for (int i = 0; i < ms / 10; i++) {
		if (last_shown(w) == want)
			return true;
		os_sleep_ms(10);
	}
	return false;
}

/* Switch the program between ticks: graphics tasks run after a tick's render
 * and before the next tick's sources, so the scene and the media source's
 * activation take effect on the same tick, as in a real scene switch. */
static void set_program(void *param)
{
	obs_set_output_source(0, param);
}

static void switch_program(obs_source_t *scene)
{
	obs_queue_task(OBS_TASK_GRAPHICS, set_program, scene, true);
}

static int group_setup(void **state)
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
		.base_width = W,
		.base_height = H,
		.output_width = W,
		.output_height = H,
		.output_format = VIDEO_FORMAT_BGRA,
		.colorspace = VIDEO_CS_SRGB,
		.range = VIDEO_RANGE_FULL,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS) {
		obs_shutdown();
		return 0;
	}

	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai)) {
		obs_shutdown();
		return 0;
	}

	obs_register_source(&ffmpeg_source);
	obs_register_source(&background_source);

	snprintf(clip_path, sizeof(clip_path), "%s/obs-media-first-frame-%llu.y4m",
		 getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp", (unsigned long long)os_gettime_ns());
	if (!write_clip(clip_path)) {
		fprintf(stderr, "could not write the test clip %s\n", clip_path);
		obs_shutdown();
		return -1;
	}

	obs_up = true;
	return 0;
}

static int group_teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_up) {
		obs_shutdown();
		os_unlink(clip_path);
		obs_up = false;
	}
	return 0;
}

enum away { AFTER_END, MID_PLAYBACK };

static void restart_shows_first_frame(bool clear_on_end, enum away away)
{
	obs_data_t *settings = obs_data_create();
	obs_data_set_bool(settings, "is_local_file", true);
	obs_data_set_string(settings, "local_file", clip_path);
	obs_data_set_bool(settings, "looping", false);
	obs_data_set_bool(settings, "restart_on_activate", true);
	obs_data_set_bool(settings, "clear_on_media_end", clear_on_end);
	obs_data_set_bool(settings, "close_when_inactive", false);
	obs_data_set_bool(settings, "hw_decode", false);
	obs_source_t *media = obs_source_create("ffmpeg_source", "media", settings, NULL);
	obs_data_release(settings);
	obs_source_t *background = obs_source_create("test_red_background", "background", NULL, NULL);
	if (!media || !background) {
		fprintf(stderr, "could not create the sources\n");
		fail();
	}

	obs_scene_t *with_media = obs_scene_create("with media");
	obs_scene_t *empty = obs_scene_create("empty");
	obs_scene_add(with_media, background);
	obs_scene_add(with_media, media);

	static struct watch w;
	memset(&w, 0, sizeof(w));
	w.last = SHOWN_BLACK;
	pthread_mutex_init(&w.mutex, NULL);
	obs_add_raw_video_callback(NULL, raw_video, &w);

	/* First playback: let it end, or leave in the middle of it. */
	switch_program(obs_scene_get_source(with_media));
	bool played;
	if (away == AFTER_END) {
		bool ended = false;
		for (int i = 0; i < 500 && !ended; i++) {
			os_sleep_ms(10);
			ended = obs_source_media_get_state(media) == OBS_MEDIA_STATE_ENDED;
		}
		played = ended && wait_for(&w, clear_on_end ? SHOWN_EMPTY : SHOWN_LAST, 2000);
	} else {
		played = wait_for(&w, SHOWN_MIDDLE, 2000);
	}

	/* Away to an empty scene, then back. */
	switch_program(obs_scene_get_source(empty));
	bool away_shown = wait_for(&w, SHOWN_BLACK, 2000);
	os_sleep_ms(300);

	pthread_mutex_lock(&w.mutex);
	w.recording = true;
	pthread_mutex_unlock(&w.mutex);

	switch_program(obs_scene_get_source(with_media));
	os_sleep_ms(1500);

	pthread_mutex_lock(&w.mutex);
	w.recording = false;
	pthread_mutex_unlock(&w.mutex);

	obs_remove_raw_video_callback(raw_video, &w);
	switch_program(NULL);
	/* the main canvas holds a reference to its scenes until they are removed */
	obs_source_remove(obs_scene_get_source(with_media));
	obs_source_remove(obs_scene_get_source(empty));
	obs_scene_release(with_media);
	obs_scene_release(empty);
	obs_source_release(media);
	obs_source_release(background);
	pthread_mutex_destroy(&w.mutex);

	if (!played || !away_shown) {
		fprintf(stderr, "setup failed: first playback shown %d, empty scene shown %d\n", played, away_shown);
		fail();
	}

	/* The activating tick is the first one that is not the empty scene. */
	size_t act = 0;
	while (act < w.count && w.seen[act] == SHOWN_BLACK)
		act++;

	/* Collapse runs for a readable log. */
	fprintf(stderr, "show nothing when playback ends %s, switched away %s: from the activating tick:",
		clear_on_end ? "on" : "off", away == AFTER_END ? "after the end" : "mid-playback");
	for (size_t i = act; i < w.count; i++) {
		if (i == act || w.seen[i] != w.seen[i - 1])
			fprintf(stderr, " %s", shown_name(w.seen[i]));
	}
	fprintf(stderr, "\n");

	if (act == w.count) {
		fprintf(stderr, "the scene with the media source never came back\n");
		fail();
	}

	/* Until the restarted playback moves past its first frame, it must
	 * show nothing but that first frame. */
	size_t empty_frames = 0, stale = 0;
	bool first = false, moved_on = false;
	for (size_t i = act; i < w.count && !moved_on; i++) {
		switch (w.seen[i]) {
		case SHOWN_FIRST:
			first = true;
			break;
		case SHOWN_MIDDLE:
			if (first)
				moved_on = true;
			else
				stale++;
			break;
		case SHOWN_LAST:
			stale++;
			break;
		case SHOWN_EMPTY:
		case SHOWN_BLACK:
			empty_frames++;
			break;
		}
	}

	if (!first || !moved_on) {
		fprintf(stderr, "playback did not restart from its first frame\n");
		fail();
	}
	if (stale) {
		fprintf(stderr, "%zu output frame(s) showed the previous playback before the restart\n", stale);
		fail();
	}
	if (empty_frames) {
		fprintf(stderr, "%zu output frame(s) showed nothing before the restarted playback\n", empty_frames);
		fail();
	}
	if (w.seen[act] != SHOWN_FIRST) {
		fprintf(stderr, "the activating tick showed %s, not the first frame\n", shown_name(w.seen[act]));
		fail();
	}
}

static void clear_on_end_after_end(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	restart_shows_first_frame(true, AFTER_END);
}

static void hold_last_frame_after_end(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	restart_shows_first_frame(false, AFTER_END);
}

static void clear_on_end_mid_playback(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	restart_shows_first_frame(true, MID_PLAYBACK);
}

static void hold_last_frame_mid_playback(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	restart_shows_first_frame(false, MID_PLAYBACK);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(clear_on_end_after_end),
		cmocka_unit_test(hold_last_frame_after_end),
		cmocka_unit_test(clear_on_end_mid_playback),
		cmocka_unit_test(hold_last_frame_mid_playback),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
