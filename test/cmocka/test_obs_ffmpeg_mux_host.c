#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include OBS_FFMPEG_MUX_HOST_SOURCE

/*
 * The FFmpeg muxer output's macOS and POSIX hosts (obs-rust/obs-studio#853):
 * the helper is "obs-ffmpeg-mux" and write errors carry no Windows Defender
 * hint, as obs-ffmpeg-mux.c chose inline. Both host files are plain C, so
 * they build and run here; OBS_FFMPEG_MUX_HOST_SOURCE picks one. The Windows
 * host needs win-version.h and is compiled by the cross check.
 */

static void test_exe_name(void **state)
{
	(void)state;
	assert_string_equal(ffmpeg_mux_host_exe_name(), "obs-ffmpeg-mux");
}

static void test_no_defender_hint(void **state)
{
	(void)state;
	assert_false(ffmpeg_mux_host_warn_defender());
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_exe_name),
		cmocka_unit_test(test_no_defender_hint),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
