/* Characterization test for libobs/util/source-profiler.c: what the per-source
 * tick and render profiler reports today, through its public API
 * (util/source-profiler.h), before it moves to Rust.
 *
 * The profiler's frame hooks (source_profiler_frame_begin/_collect, the tick
 * and render hooks, async frame received, remove source) are declared in
 * obs-internal.h and are not exported, so the cases drive them the way the
 * application does: core video runs with the OpenGL module, one scene with a
 * synchronous and an asynchronous source is the output, and the cases wait
 * for the video thread's tick of the synchronous source (an event, no sleeps).
 * GPU timing stays off: source_profiler_gpu_enable() is only ever called while
 * the profiler is disabled, which the C ignores.
 *
 * Timings are real, so the cases pin relations (max >= average, a sum of
 * passes >= the first pass, best <= worst) and the values that are exact:
 * zeroed GPU and async fields, untouched or zeroed results, NULL results.
 *
 * Core video needs a graphics module, and libobs needs an X display on Linux,
 * so the cases that need video skip without one (run under xvfb-run).
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <stdio.h>
#include <string.h>

#include <obs.h>
#include <util/bmem.h>
#include <util/platform.h>
#include <util/source-profiler.h>
#include <util/threading.h>

#define SYNC_ID "test_source_profiler_sync"
#define ASYNC_ID "test_source_profiler_async"
#define QUIET_ID "test_source_profiler_quiet"
#define TIMEOUT_MS 5000

/* Frames to wait after enabling: the enable applies at the next frame begin,
 * a sample is collected one frame later (FRAME_BUFFER_SIZE - 1), and the
 * waited tick may belong to a frame that began before the call. */
#define FRAMES_TO_COLLECT 6
/* Frames to wait for a state change applied at the end of a frame (disable)
 * or by a graphics task (remove). */
#define FRAMES_TO_APPLY 3

static int obs_up;
static os_event_t *tick_event;
static obs_scene_t *scene;
static obs_source_t *sync_src;
static obs_source_t *async_src;

/* ------------------------------------------------------------------------- */
/* sources */

static const char *src_name(void *type_data)
{
	(void)type_data;
	return "source profiler test source";
}

static void *src_create(obs_data_t *settings, obs_source_t *source)
{
	(void)settings;
	return source;
}

static void src_destroy(void *data)
{
	(void)data;
}

static uint32_t src_size(void *data)
{
	(void)data;
	return 8;
}

static void src_render(void *data, gs_effect_t *effect)
{
	(void)data;
	(void)effect;
}

/* The video thread ticks every source once per frame. */
static void sync_tick(void *data, float seconds)
{
	(void)data;
	(void)seconds;
	os_event_signal(tick_event);
}

static void register_sources(void)
{
	struct obs_source_info sync_info = {
		.id = SYNC_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_VIDEO,
		.get_name = src_name,
		.create = src_create,
		.destroy = src_destroy,
		.get_width = src_size,
		.get_height = src_size,
		.video_render = src_render,
		.video_tick = sync_tick,
	};
	obs_register_source(&sync_info);

	struct obs_source_info quiet_info = sync_info;
	quiet_info.id = QUIET_ID;
	quiet_info.video_tick = NULL;
	obs_register_source(&quiet_info);

	struct obs_source_info async_info = {
		.id = ASYNC_ID,
		.type = OBS_SOURCE_TYPE_INPUT,
		.output_flags = OBS_SOURCE_ASYNC_VIDEO,
		.get_name = src_name,
		.create = src_create,
		.destroy = src_destroy,
	};
	obs_register_source(&async_info);
}

static int reset_video(void)
{
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
	return obs_reset_video(&ovi);
}

/* ------------------------------------------------------------------------- */
/* helpers */

static void wait_frames(int frames)
{
	for (int i = 0; i < frames; i++) {
		if (os_event_timedwait(tick_event, TIMEOUT_MS) != 0) {
			fprintf(stderr, "no video tick within %d ms\n", TIMEOUT_MS);
			fail();
		}
	}
}

static void output_async_frame(uint64_t timestamp)
{
	uint8_t pixels[4 * 4 * 4];
	memset(pixels, 0x80, sizeof(pixels));

	struct obs_source_frame frame = {0};
	frame.data[0] = pixels;
	frame.linesize[0] = 4 * 4;
	frame.width = 4;
	frame.height = 4;
	frame.format = VIDEO_FORMAT_BGRA;
	frame.timestamp = timestamp;
	obs_source_output_video(async_src, &frame);
}

static void fill_sentinel(profiler_result_t *result)
{
	memset(result, 0xAB, sizeof(*result));
}

static void assert_sentinel(const profiler_result_t *result)
{
	profiler_result_t sentinel;
	fill_sentinel(&sentinel);
	assert_memory_equal(result, &sentinel, sizeof(sentinel));
}

static void assert_zeroed(const profiler_result_t *result)
{
	profiler_result_t zero;
	memset(&zero, 0, sizeof(zero));
	assert_memory_equal(result, &zero, sizeof(zero));
}

static void assert_cpu_relations(const profiler_result_t *r)
{
	assert_true(r->tick_max >= r->tick_avg);
	assert_true(r->render_max >= r->render_avg);
	/* the average sum of all passes is at least the average first pass */
	assert_true(r->render_sum >= r->render_avg);
}

static void assert_gpu_zero(const profiler_result_t *r)
{
	assert_int_equal(r->render_gpu_avg, 0);
	assert_int_equal(r->render_gpu_max, 0);
	assert_int_equal(r->render_gpu_sum, 0);
}

static void assert_async_zero(const profiler_result_t *r)
{
	assert_true(r->async_input == 0.0);
	assert_true(r->async_rendered == 0.0);
	assert_int_equal(r->async_input_best, 0);
	assert_int_equal(r->async_input_worst, 0);
	assert_int_equal(r->async_rendered_best, 0);
	assert_int_equal(r->async_rendered_worst, 0);
}

/* ------------------------------------------------------------------------- */
/* cases */

/* Before anything enables it the profiler reports nothing and leaves the
 * caller's result untouched (the check comes before the memset). */
static void disabled_reports_nothing(void **state)
{
	(void)state;
	profiler_result_t r;

	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(NULL, &r));
	assert_sentinel(&r);
	assert_null(source_profiler_get_result(NULL));
	assert_false(source_profiler_fill_result(NULL, NULL));

	if (!obs_up)
		return;

	wait_frames(FRAMES_TO_APPLY);
	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(sync_src, &r));
	assert_sentinel(&r);
	assert_null(source_profiler_get_result(sync_src));
	assert_false(source_profiler_fill_result(sync_src, NULL));
}

/* source_profiler_gpu_enable(true) while the profiler is not enabled is
 * dropped (gpu_enable_next = enable && enable_next), so enabling the profiler
 * afterwards measures CPU only. */
static void cpu_profile_fills_result(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	source_profiler_gpu_enable(true);
	source_profiler_enable(true);
	wait_frames(FRAMES_TO_COLLECT);

	profiler_result_t r;
	fill_sentinel(&r);
	assert_true(source_profiler_fill_result(sync_src, &r));
	/* the tick signals an event, which takes time */
	assert_true(r.tick_max > 0);
	assert_cpu_relations(&r);
	assert_gpu_zero(&r);
	/* not an async source: the fps fields stay zero */
	assert_async_zero(&r);

	profiler_result_t *got = source_profiler_get_result(sync_src);
	assert_non_null(got);
	assert_cpu_relations(got);
	assert_gpu_zero(got);
	assert_async_zero(got);
	bfree(got);

	/* a NULL result is refused while enabled too */
	assert_false(source_profiler_fill_result(sync_src, NULL));
}

/* A source the profiler has no entry for: false, and the result is zeroed
 * (enabled, so the memset runs). The pointer is only a hash key, never
 * dereferenced. */
static void unknown_source_zeroes_result(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	int not_a_source = 0;
	obs_source_t *unknown = (obs_source_t *)&not_a_source;
	profiler_result_t r;

	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(unknown, &r));
	assert_zeroed(&r);
	assert_null(source_profiler_get_result(unknown));

	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(NULL, &r));
	assert_zeroed(&r);
	assert_null(source_profiler_get_result(NULL));
}

/* An async source gets the input fps from the frames it was given: with
 * three frames the deltas between them give a positive fps, and best is at
 * most worst. The rendered fps comes from the last rendered timestamp per
 * frame and is zero until two different ones were rendered. */
static void async_source_reports_fps(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	profiler_result_t r;
	fill_sentinel(&r);
	assert_true(source_profiler_fill_result(async_src, &r));
	assert_cpu_relations(&r);
	assert_gpu_zero(&r);
	assert_true(r.async_input == 0.0);
	assert_int_equal(r.async_input_best, 0);
	assert_int_equal(r.async_input_worst, 0);

	output_async_frame(1000000000ULL);
	output_async_frame(1033333333ULL);
	output_async_frame(1066666666ULL);

	fill_sentinel(&r);
	assert_true(source_profiler_fill_result(async_src, &r));
	assert_true(r.async_input > 0.0);
	assert_true(r.async_input_best > 0);
	assert_true(r.async_input_best <= r.async_input_worst);
	if (r.async_rendered == 0.0) {
		assert_int_equal(r.async_rendered_best, 0);
		assert_int_equal(r.async_rendered_worst, 0);
	} else {
		assert_true(r.async_rendered_best > 0);
		assert_true(r.async_rendered_best <= r.async_rendered_worst);
	}
	assert_gpu_zero(&r);
}

static void release_task(void *param)
{
	obs_source_release(param);
}

/* Destroying a source schedules the deletion of its samples and entry as a
 * graphics task. The last release runs here as a graphics task itself, so
 * the deletion runs at once, inside it (obs_queue_task from the graphics
 * thread). Releasing it from another thread instead lets the graphics thread
 * collect the source's last sample after it was freed and before the task
 * ran; that is not exercised. Afterwards the source is unknown: false and a
 * zeroed result (its address is only a hash key). */
static void destroyed_source_is_forgotten(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	obs_source_t *quiet = obs_source_create(QUIET_ID, "source profiler removable", NULL, NULL);
	assert_non_null(quiet);
	wait_frames(FRAMES_TO_COLLECT);

	profiler_result_t r;
	assert_true(source_profiler_fill_result(quiet, &r));
	assert_cpu_relations(&r);

	obs_source_remove(quiet);
	obs_queue_task(OBS_TASK_GRAPHICS, release_task, quiet, true);

	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(quiet, &r));
	assert_zeroed(&r);
	assert_null(source_profiler_get_result(quiet));

	/* the other sources keep their entries */
	wait_frames(FRAMES_TO_APPLY);
	assert_true(source_profiler_fill_result(sync_src, &r));
}

/* Disabling applies at the end of the next frame and drops every entry;
 * the result is then left untouched again. Enabling again starts over. */
static void disable_drops_entries(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	source_profiler_enable(false);
	wait_frames(FRAMES_TO_APPLY);

	profiler_result_t r;
	fill_sentinel(&r);
	assert_false(source_profiler_fill_result(sync_src, &r));
	assert_sentinel(&r);
	assert_null(source_profiler_get_result(async_src));

	source_profiler_enable(true);
	wait_frames(FRAMES_TO_COLLECT);
	assert_true(source_profiler_fill_result(sync_src, &r));
	assert_cpu_relations(&r);
	assert_gpu_zero(&r);
	/* the async input timestamps went with the old entry */
	assert_true(source_profiler_fill_result(async_src, &r));
	assert_true(r.async_input == 0.0);

	source_profiler_enable(false);
	wait_frames(FRAMES_TO_APPLY);
	assert_false(source_profiler_fill_result(sync_src, &r));
}

/* ------------------------------------------------------------------------- */

static int group_setup(void **state)
{
	(void)state;
	if (os_event_init(&tick_event, OS_EVENT_TYPE_AUTO) != 0)
		return -1;

	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up)
		return 0;
	struct obs_audio_info oai = {.samples_per_sec = 48000, .speakers = SPEAKERS_STEREO};
	if (!obs_reset_audio(&oai)) {
		fprintf(stderr, "obs_reset_audio failed\n");
		return -1;
	}
	int ret = reset_video();
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "obs_reset_video failed: %d\n", ret);
		obs_shutdown();
		obs_up = 0;
		return 0;
	}

	register_sources();
	sync_src = obs_source_create(SYNC_ID, "source profiler sync", NULL, NULL);
	async_src = obs_source_create(ASYNC_ID, "source profiler async", NULL, NULL);
	scene = obs_scene_create("source profiler scene");
	if (!sync_src || !async_src || !scene)
		return -1;
	obs_scene_add(scene, sync_src);
	obs_scene_add(scene, async_src);
	obs_set_output_source(0, obs_scene_get_source(scene));
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up) {
		source_profiler_enable(false);
		obs_set_output_source(0, NULL);
		obs_scene_release(scene);
		obs_wait_for_destroy_queue();
		obs_source_release(async_src);
		obs_source_release(sync_src);
		obs_wait_for_destroy_queue();
		obs_shutdown();
	}
	os_event_destroy(tick_event);
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(disabled_reports_nothing),      cmocka_unit_test(cpu_profile_fills_result),
		cmocka_unit_test(unknown_source_zeroes_result),  cmocka_unit_test(async_source_reports_fps),
		cmocka_unit_test(destroyed_source_is_forgotten), cmocka_unit_test(disable_drops_entries),
	};
	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
