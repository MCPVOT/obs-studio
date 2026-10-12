/*
 * Double-rate deinterlacing must output the fields of an interlaced source in
 * the order they were captured, for both field orders (upstream
 * obsproject/obs-studio#6061: Yadif 2x on bottom-field-first video jumps back
 * in time on every other output frame).
 *
 * A test async source outputs interlaced BGRA frames at 30 fps into a 60 fps
 * canvas. Every field carries its capture index t (0, 1, 2, ... in time
 * order) in the red and blue channels of its rows, and its parity in green
 * (top rows dark, bottom rows bright). Each output frame is classified by the
 * field it kept: green tells the parity (the interpolated rows of a flat
 * field take the kept field's green), the kept row gives t. The kept fields
 * must never go back in time, and the 2x modes must deliver most fields.
 *
 * Needs core video, so it runs under xvfb-run and skips without a display.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <obs.h>
#include <util/platform.h>
#include <util/threading.h>

#define W 64
#define H 64
#define FPS 30
#define FRAMES 36

/* Field t is drawn as R = R_BASE + STEP * (t / 8), B = R_BASE + STEP * (t % 8). */
#define R_BASE 32
#define STEP 24
#define G_TOP 64
#define G_BOTTOM 192
#define TOLERANCE 6

#define MAX_SEEN 1024

static bool obs_up;

struct watch {
	pthread_mutex_t mutex;
	bool recording;
	size_t count;
	int seen[MAX_SEEN]; /* field index, or -1 for an unrecognised frame */
};

static int snap(int v)
{
	int i = (v - R_BASE + STEP / 2) / STEP;
	if (v < R_BASE - TOLERANCE || i < 0)
		return -1;
	int d = v - (R_BASE + STEP * i);
	return (d >= -TOLERANCE && d <= TOLERANCE) ? i : -1;
}

static int parity_of(int g)
{
	if (g >= G_TOP - TOLERANCE * 2 && g <= G_TOP + TOLERANCE * 2)
		return 0;
	if (g >= G_BOTTOM - TOLERANCE * 2 && g <= G_BOTTOM + TOLERANCE * 2)
		return 1;
	return -1;
}

/* BGRA output: returns the index of the field this frame kept, or -1. */
static int classify(const struct video_data *frame)
{
	const uint8_t *row0 = frame->data[0] + (H / 2) * frame->linesize[0] + (W / 2) * 4;
	const uint8_t *row1 = row0 + frame->linesize[0];
	int p0 = parity_of(row0[1]), p1 = parity_of(row1[1]);
	if (p0 < 0 || p0 != p1)
		return -1;

	const uint8_t *kept = p0 == 0 ? row0 : row1;
	int hi = snap(kept[2]), lo = snap(kept[0]);
	if (hi < 0 || lo < 0 || lo > 7)
		return -1;
	return hi * 8 + lo;
}

static void raw_video(void *param, struct video_data *frame)
{
	struct watch *w = param;
	int field = classify(frame);

	pthread_mutex_lock(&w->mutex);
	if (w->recording && w->count < MAX_SEEN)
		w->seen[w->count++] = field;
	pthread_mutex_unlock(&w->mutex);
}

static const char *interlaced_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "interlaced test source";
}

static void *interlaced_create(obs_data_t *settings, obs_source_t *source)
{
	UNUSED_PARAMETER(settings);
	return source;
}

static void interlaced_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static struct obs_source_info interlaced_source = {
	.id = "interlaced_test_source",
	.type = OBS_SOURCE_TYPE_INPUT,
	.output_flags = OBS_SOURCE_ASYNC_VIDEO,
	.get_name = interlaced_name,
	.create = interlaced_create,
	.destroy = interlaced_destroy,
};

static void fill_rows(uint8_t *pixels, int parity, int t)
{
	for (int y = parity; y < H; y += 2) {
		uint8_t *row = pixels + y * W * 4;
		for (int x = 0; x < W; x++) {
			row[x * 4 + 0] = (uint8_t)(R_BASE + STEP * (t % 8));
			row[x * 4 + 1] = parity ? G_BOTTOM : G_TOP;
			row[x * 4 + 2] = (uint8_t)(R_BASE + STEP * (t / 8));
			row[x * 4 + 3] = 255;
		}
	}
}

/* Pushes FRAMES interlaced frames at FPS, timestamped like a capture card. */
static void play(obs_source_t *source, bool top_first)
{
	static uint8_t pixels[W * H * 4];
	const uint64_t period = 1000000000ULL / FPS;
	const uint64_t start = os_gettime_ns();

	for (int n = 0; n < FRAMES; n++) {
		/* the first field of frame n was captured at 2n, the second at 2n + 1 */
		fill_rows(pixels, top_first ? 0 : 1, 2 * n);
		fill_rows(pixels, top_first ? 1 : 0, 2 * n + 1);

		uint64_t ts = start + n * period;
		struct obs_source_frame frame = {
			.data = {pixels},
			.linesize = {W * 4},
			.width = W,
			.height = H,
			.format = VIDEO_FORMAT_BGRA,
			.full_range = true,
			.timestamp = ts,
		};
		os_sleepto_ns(ts);
		obs_source_output_video(source, &frame);
	}
	os_sleep_ms(300);
}

static const char *mode_name(enum obs_deinterlace_mode mode)
{
	switch (mode) {
	case OBS_DEINTERLACE_MODE_YADIF_2X:
		return "yadif 2x";
	case OBS_DEINTERLACE_MODE_LINEAR_2X:
		return "linear 2x";
	case OBS_DEINTERLACE_MODE_RETRO:
		return "discard 2x (retro)";
	default:
		return "?";
	}
}

static void check_field_order(enum obs_deinterlace_mode mode, bool top_first)
{
	const char *order = top_first ? "top field first" : "bottom field first";

	obs_source_t *source = obs_source_create("interlaced_test_source", "interlaced", NULL, NULL);
	obs_source_set_deinterlace_mode(source, mode);
	obs_source_set_deinterlace_field_order(source, top_first ? OBS_DEINTERLACE_FIELD_ORDER_TOP
								 : OBS_DEINTERLACE_FIELD_ORDER_BOTTOM);

	obs_scene_t *scene = obs_scene_create("scene");
	obs_scene_add(scene, source);

	static struct watch w;
	memset(&w, 0, sizeof(w));
	pthread_mutex_init(&w.mutex, NULL);
	obs_add_raw_video_callback(NULL, raw_video, &w);
	obs_set_output_source(0, obs_scene_get_source(scene));

	pthread_mutex_lock(&w.mutex);
	w.recording = true;
	pthread_mutex_unlock(&w.mutex);

	play(source, top_first);

	pthread_mutex_lock(&w.mutex);
	w.recording = false;
	pthread_mutex_unlock(&w.mutex);

	/* Let the output drain to black so the next case starts clean. */
	obs_set_output_source(0, NULL);
	os_sleep_ms(250);
	obs_remove_raw_video_callback(raw_video, &w);
	obs_source_remove(obs_scene_get_source(scene));
	obs_scene_release(scene);
	obs_source_release(source);
	pthread_mutex_destroy(&w.mutex);

	/* Collapse repeats: the fields in the order they were shown. */
	int shown[MAX_SEEN];
	size_t num = 0, unknown = 0, backwards = 0;
	for (size_t i = 0; i < w.count; i++) {
		if (w.seen[i] < 0) {
			unknown++;
			continue;
		}
		if (num && shown[num - 1] == w.seen[i])
			continue;
		if (num && w.seen[i] < shown[num - 1])
			backwards++;
		shown[num++] = w.seen[i];
	}

	fprintf(stderr, "%s, %s: %zu output frames, %zu unrecognised, %zu fields shown, %zu steps back in time:",
		mode_name(mode), order, w.count, unknown, num, backwards);
	for (size_t i = 0; i < num; i++)
		fprintf(stderr, " %d", shown[i]);
	fprintf(stderr, "\n");

	if (num < FRAMES) {
		fprintf(stderr, "%s, %s: only %zu of %d fields were shown\n", mode_name(mode), order, num, 2 * FRAMES);
		fail();
	}
	if (backwards) {
		fprintf(stderr, "%s, %s: the output went back in time %zu times\n", mode_name(mode), order, backwards);
		fail();
	}
}

static void yadif_2x_top_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_YADIF_2X, true);
}

static void yadif_2x_bottom_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_YADIF_2X, false);
}

static void linear_2x_top_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_LINEAR_2X, true);
}

static void linear_2x_bottom_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_LINEAR_2X, false);
}

static void discard_2x_top_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_RETRO, true);
}

static void discard_2x_bottom_first(void **state)
{
	UNUSED_PARAMETER(state);
	if (!obs_up)
		skip();
	check_field_order(OBS_DEINTERLACE_MODE_RETRO, false);
}

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	/* obs_startup() needs a display on Linux (run under xvfb-run); the
	 * cases skip without one. */
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}

	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = 2 * FPS,
		.fps_den = 1,
		.base_width = W,
		.base_height = H,
		.output_width = W,
		.output_height = H,
		.output_format = VIDEO_FORMAT_BGRA,
		.colorspace = VIDEO_CS_SRGB,
		.range = VIDEO_RANGE_FULL,
		.scale_type = OBS_SCALE_POINT,
	};
	int ret = obs_reset_video(&ovi);
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "skipped: obs_reset_video failed: %d\n", ret);
		obs_shutdown();
		obs_up = false;
		return 0;
	}

	obs_register_source(&interlaced_source);
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(yadif_2x_top_first),   cmocka_unit_test(yadif_2x_bottom_first),
		cmocka_unit_test(linear_2x_top_first),  cmocka_unit_test(linear_2x_bottom_first),
		cmocka_unit_test(discard_2x_top_first), cmocka_unit_test(discard_2x_bottom_first),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
