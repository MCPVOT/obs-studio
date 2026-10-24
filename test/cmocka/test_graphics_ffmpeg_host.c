#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "libobs/graphics/graphics-ffmpeg-host-posix.c.h"

/*
 * The image loader's POSIX host (obs-rust/obs-studio#872): when FFmpeg
 * cannot decode a file there is no second decoder, so the fallback loads
 * nothing and leaves the outputs alone. The Windows host (WIC, for JPEG XR)
 * is compiled by the cross check.
 */

static void test_no_fallback(void **state)
{
	(void)state;
	enum gs_color_format format = GS_UNKNOWN;
	enum gs_color_space space = GS_CS_SRGB;
	uint32_t cx = 7, cy = 9;

	assert_null(gs_image_host_fallback_load("picture.jxr", &format, &cx, &cy, &space));
	assert_int_equal(format, GS_UNKNOWN);
	assert_int_equal(cx, 7);
	assert_int_equal(cy, 9);
	assert_int_equal(space, GS_CS_SRGB);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_no_fallback),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
