/*
 * "Skipped frames due to encoding lag" must count encoders that rescale the
 * output (upstream obsproject/obs-studio#11078, also #10744, #11551 and
 * #11907).
 *
 * Stats, the status bar's overload warning and obs-websocket's GetStats read
 * the frame counters of obs_get_video(), the main video mix. An encoder with
 * a GPU scaled size (Settings > Output > Rescale Output) gets an
 * encoder-only video mix instead (maybe_set_up_gpu_rescale() in
 * obs-encoder.c), and that mix's video_t counts its frames. The main mix
 * stays at 0 / 0, so Stats shows no frames and an overloaded encoder never
 * raises the warning. obs_get_video_frame_counts() reports the frames of the
 * main canvas wherever they are encoded.
 *
 * Each case starts an output with a raw video encoder on a 64x64 canvas at
 * 30 fps for about a second, scaled to 32x32 or not, and reads the counts
 * while it runs. A "slow" encoder takes 100 ms per frame, so the video
 * thread has to skip frames. Guards: an unscaled encoder (counted before the
 * fix too), two rescaling encoders that share one mix (counted once), and an
 * encoder on a second canvas (not part of the main canvas, not counted).
 *
 * Core video needs a graphics module and libobs needs an X display on
 * Linux, so the cases run under xvfb-run and skip without a display.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <stdlib.h>

#include <obs.h>
#include <util/platform.h>
#include <util/threading.h>

#define FPS 30
#define SETTLE_MS 500
#define RUN_MS 1500
#define SLOW_MS 100

/* ------------------------------------------------------------------------ */
/* Raw video encoder that takes "delay_ms" per frame and emits nothing.     */

/* Set before stopping: a lagging encoder keeps the video thread busy, and
 * the output's stop could wait a long time to disconnect it. */
static volatile bool encoders_fast;

static const char *venc_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "lag test encoder";
}

static void *venc_create(obs_data_t *settings, obs_encoder_t *encoder)
{
	UNUSED_PARAMETER(encoder);
	int *delay_ms = bzalloc(sizeof(*delay_ms));
	*delay_ms = (int)obs_data_get_int(settings, "delay_ms");
	return delay_ms;
}

static void venc_destroy(void *data)
{
	bfree(data);
}

static bool venc_encode(void *data, struct encoder_frame *frame, struct encoder_packet *packet, bool *received_packet)
{
	int *delay_ms = data;
	UNUSED_PARAMETER(frame);
	UNUSED_PARAMETER(packet);
	if (*delay_ms && !os_atomic_load_bool(&encoders_fast))
		os_sleep_ms((uint32_t)*delay_ms);
	*received_packet = false;
	return true;
}

static struct obs_encoder_info venc_info = {
	.id = "test_lag_venc",
	.type = OBS_ENCODER_VIDEO,
	.codec = "h264",
	.get_name = venc_name,
	.create = venc_create,
	.destroy = venc_destroy,
	.encode = venc_encode,
};

/* Video-only encoded output that starts its encoder, as real outputs do. */
static const char *out_name(void *unused)
{
	UNUSED_PARAMETER(unused);
	return "lag test output";
}

static void *out_create(obs_data_t *settings, obs_output_t *output)
{
	UNUSED_PARAMETER(settings);
	return output;
}

static void out_destroy(void *data)
{
	UNUSED_PARAMETER(data);
}

static bool out_start(void *data)
{
	obs_output_t *output = data;
	if (!obs_output_can_begin_data_capture(output, 0))
		return false;
	if (!obs_output_initialize_encoders(output, 0))
		return false;
	return obs_output_begin_data_capture(output, 0);
}

static void out_stop(void *data, uint64_t ts)
{
	UNUSED_PARAMETER(ts);
	obs_output_end_data_capture(data);
}

static void out_packet(void *data, struct encoder_packet *packet)
{
	UNUSED_PARAMETER(data);
	UNUSED_PARAMETER(packet);
}

static struct obs_output_info out_info = {
	.id = "test_lag_output",
	.flags = OBS_OUTPUT_VIDEO | OBS_OUTPUT_ENCODED,
	.encoded_video_codecs = "h264",
	.get_name = out_name,
	.create = out_create,
	.destroy = out_destroy,
	.start = out_start,
	.stop = out_stop,
	.encoded_packet = out_packet,
};

/* ------------------------------------------------------------------------ */

static struct obs_video_info canvas_ovi(void)
{
	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = FPS,
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
	return ovi;
}

static bool init_core(void)
{
	if (!obs_startup("en-US", NULL, NULL))
		return false;

	struct obs_video_info ovi = canvas_ovi();
	if (obs_reset_video(&ovi) != OBS_VIDEO_SUCCESS)
		return false;

	obs_register_encoder(&venc_info);
	obs_register_output(&out_info);
	return true;
}

struct run {
	obs_encoder_t *encoder;
	obs_output_t *output;
};

static void run_start(struct run *run, video_t *video, bool rescale, int delay_ms)
{
	os_atomic_set_bool(&encoders_fast, false);
	obs_data_t *settings = obs_data_create();
	obs_data_set_int(settings, "delay_ms", delay_ms);
	run->encoder = obs_video_encoder_create("test_lag_venc", "venc", settings, NULL);
	obs_data_release(settings);
	if (rescale) {
		obs_encoder_set_gpu_scale_type(run->encoder, OBS_SCALE_BICUBIC);
		obs_encoder_set_scaled_size(run->encoder, 32, 32);
	}
	obs_encoder_set_video(run->encoder, video);

	run->output = obs_output_create("test_lag_output", "out", NULL, NULL);
	obs_output_set_video_encoder(run->output, run->encoder);
	if (!obs_output_start(run->output)) {
		fprintf(stderr, "the test output did not start: %s\n", obs_output_get_last_error(run->output));
		fail();
	}
}

static void run_stop(struct run *run)
{
	os_atomic_set_bool(&encoders_fast, true);
	obs_output_force_stop(run->output);
	obs_output_release(run->output);
	obs_encoder_release(run->encoder);
}

struct counts {
	uint32_t total;
	uint32_t skipped;
};

/* Counts while the outputs run: a rescaling encoder's mix (and its
 * counters) goes away when it stops. Starts after the outputs have settled,
 * so the baseline already includes any new mix. */
static struct counts measure(void)
{
	struct counts before, after;

	os_sleep_ms(SETTLE_MS);
	obs_get_video_frame_counts(&before.total, &before.skipped);
	os_sleep_ms(RUN_MS);
	obs_get_video_frame_counts(&after.total, &after.skipped);

	struct counts delta = {after.total - before.total, after.skipped - before.skipped};
	return delta;
}

static bool core_ready;

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	const char *display = getenv("DISPLAY");
	if (display && *display)
		core_ready = init_core();
	if (!core_ready)
		fprintf(stderr, "no display or no core video: the cases skip\n");
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	if (obs_initialized())
		obs_shutdown();
	return 0;
}

/* The counters advance as the video thread hands frames to the encoders,
 * once per frame, skipped or not: 45 in RUN_MS for a fast encoder, about
 * 15 for a slow one, of which most are skipped once its frame cache has
 * filled. The bounds leave room for a slow software-GL machine. */
#define MIN_FRAMES (FPS * RUN_MS / 1000 / 2)
#define MIN_SLOW_FRAMES 5
#define MIN_SKIPPED 3

static void check_frames(const char *what, bool rescale, int delay_ms)
{
	if (!core_ready)
		skip();

	struct run run;
	run_start(&run, obs_get_video(), rescale, delay_ms);
	struct counts c = measure();
	run_stop(&run);

	fprintf(stderr, "%s: %u frames, %u skipped in %d ms\n", what, c.total, c.skipped, RUN_MS);
	int min_frames = delay_ms ? MIN_SLOW_FRAMES : MIN_FRAMES;
	if (c.total < (uint32_t)min_frames) {
		fprintf(stderr, "%s: expected at least %d frames\n", what, min_frames);
		fail();
	}
	if (delay_ms && c.skipped < MIN_SKIPPED) {
		fprintf(stderr, "%s: a %d ms/frame encoder at %d fps must skip frames\n", what, delay_ms, FPS);
		fail();
	}
	if (!delay_ms && c.skipped > c.total / 2) {
		fprintf(stderr, "%s: an idle encoder should not skip most frames\n", what);
		fail();
	}
}

static void unscaled_encoder_frames_counted(void **state)
{
	UNUSED_PARAMETER(state);
	check_frames("unscaled", false, 0);
}

static void unscaled_slow_encoder_skips_counted(void **state)
{
	UNUSED_PARAMETER(state);
	check_frames("unscaled, slow", false, SLOW_MS);
}

static void rescaled_encoder_frames_counted(void **state)
{
	UNUSED_PARAMETER(state);
	check_frames("rescaled", true, 0);
}

static void rescaled_slow_encoder_skips_counted(void **state)
{
	UNUSED_PARAMETER(state);
	check_frames("rescaled, slow", true, SLOW_MS);
}

/* Two rescaling encoders with the same size share one encoder-only mix
 * (stream and recording both rescaled to 720p, say). Its frames count once,
 * as two unscaled encoders on the main mix count once. */
static void shared_rescale_mix_counted_once(void **state)
{
	UNUSED_PARAMETER(state);
	if (!core_ready)
		skip();

	struct run a, b;
	run_start(&a, obs_get_video(), true, 0);
	run_start(&b, obs_get_video(), true, 0);
	bool shared = obs_encoder_video(a.encoder) == obs_encoder_video(b.encoder);
	struct counts c = measure();
	run_stop(&b);
	run_stop(&a);

	fprintf(stderr, "two rescaled encoders, %s mix: %u frames in %d ms\n", shared ? "one" : "two", c.total, RUN_MS);
	if (!shared) {
		fprintf(stderr, "the encoders were expected to share one rescale mix\n");
		fail();
	}
	if (c.total < MIN_FRAMES || c.total > FPS * RUN_MS / 1000 * 3 / 2) {
		fprintf(stderr, "expected about %d frames, counted once\n", FPS * RUN_MS / 1000);
		fail();
	}
}

/* An encoder on another canvas (a vertical canvas plugin, for example) is
 * not part of the main canvas's encoding lag. */
static void other_canvas_not_counted(void **state)
{
	UNUSED_PARAMETER(state);
	if (!core_ready)
		skip();

	struct obs_video_info ovi = canvas_ovi();
	obs_canvas_t *canvas = obs_canvas_create_private("other canvas", &ovi, DEVICE);
	if (!canvas) {
		fprintf(stderr, "could not create a second canvas\n");
		fail();
	}

	struct run run;
	run_start(&run, obs_canvas_get_video(canvas), true, SLOW_MS);
	struct counts c = measure();
	run_stop(&run);
	obs_canvas_remove(canvas);
	obs_canvas_release(canvas);

	fprintf(stderr, "second canvas: %u frames, %u skipped counted for the main canvas\n", c.total, c.skipped);
	if (c.total || c.skipped) {
		fprintf(stderr, "frames of another canvas were counted\n");
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(unscaled_encoder_frames_counted),
		cmocka_unit_test(unscaled_slow_encoder_skips_counted),
		cmocka_unit_test(rescaled_encoder_frames_counted),
		cmocka_unit_test(rescaled_slow_encoder_skips_counted),
		cmocka_unit_test(shared_rescale_mix_counted_once),
		cmocka_unit_test(other_canvas_not_counted),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
