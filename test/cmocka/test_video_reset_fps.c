/* obs_reset_video() refuses a frame rate with a zero numerator or
 * denominator before anything is freed (#1713). The frame interval is
 * fps_den / fps_num, so fps_num 0 divided by zero; fps_den 0 gave a zero
 * interval. The old video keeps running. */

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <setjmp.h>
#include <stdio.h>
#include <cmocka.h>

#include <obs.h>

static bool obs_up;

static struct obs_video_info make_ovi(uint32_t fps_num, uint32_t fps_den)
{
	struct obs_video_info ovi = {
		.graphics_module = GRAPHICS_MODULE_PATH,
		.fps_num = fps_num,
		.fps_den = fps_den,
		.base_width = 64,
		.base_height = 64,
		.output_width = 64,
		.output_height = 64,
		.output_format = VIDEO_FORMAT_NV12,
		.colorspace = VIDEO_CS_709,
		.range = VIDEO_RANGE_PARTIAL,
		.gpu_conversion = true,
		.scale_type = OBS_SCALE_BICUBIC,
	};
	return ovi;
}

static void zero_fps_refused_before_teardown(void **state)
{
	(void)state;
	if (!obs_up)
		skip();

	const uint32_t bad[][2] = {{0, 1}, {30, 0}, {0, 0}};
	for (size_t i = 0; i < sizeof(bad) / sizeof(bad[0]); i++) {
		struct obs_video_info ovi = make_ovi(bad[i][0], bad[i][1]);
		assert_int_equal(obs_reset_video(&ovi), OBS_VIDEO_INVALID_PARAM);

		/* the old video is still there */
		assert_non_null(obs_get_video());
		struct obs_video_info got;
		assert_true(obs_get_video_info(&got));
		assert_int_equal(got.fps_num, 60);
		assert_int_equal(got.fps_den, 1);
	}

	/* a later reset with a valid rate works */
	struct obs_video_info later = make_ovi(30, 1);
	assert_int_equal(obs_reset_video(&later), OBS_VIDEO_SUCCESS);
}

static int group_setup(void **state)
{
	(void)state;
	obs_up = obs_startup("en-US", NULL, NULL);
	if (!obs_up) {
		fprintf(stderr, "skipped: obs_startup failed (no display?)\n");
		return 0;
	}
	struct obs_video_info ovi = make_ovi(60, 1);
	int ret = obs_reset_video(&ovi);
	if (ret != OBS_VIDEO_SUCCESS) {
		fprintf(stderr, "skipped: obs_reset_video failed: %d\n", ret);
		obs_shutdown();
		obs_up = false;
	}
	return 0;
}

static int group_teardown(void **state)
{
	(void)state;
	if (obs_up)
		obs_shutdown();
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(zero_fps_refused_before_teardown),
	};

	return cmocka_run_group_tests(tests, group_setup, group_teardown);
}
