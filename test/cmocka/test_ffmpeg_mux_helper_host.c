#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/*
 * The ffmpeg-mux helper's POSIX host (obs-rust/obs-studio#853): its main()
 * hands argc and argv to the shared ffmpeg_mux_main() unchanged and returns
 * its result. The host is renamed on inclusion so cmocka keeps main(). The
 * Windows host (wmain, UTF-16 arguments, DLL search hardening) needs
 * windows.h and is compiled by the cross check.
 */

static int seen_argc;
static char **seen_argv;

int ffmpeg_mux_main(int argc, char *argv[])
{
	seen_argc = argc;
	seen_argv = argv;
	return 7;
}

#define main ffmpeg_mux_host_entry
#include "plugins/obs-ffmpeg/ffmpeg-mux/ffmpeg-mux-host-posix.c.h"
#undef main

static void test_forwards_arguments(void **state)
{
	(void)state;
	char a0[] = "obs-ffmpeg-mux", a1[] = "out.mkv";
	char *argv[] = {a0, a1, NULL};

	assert_int_equal(ffmpeg_mux_host_entry(2, argv), 7);
	assert_int_equal(seen_argc, 2);
	assert_ptr_equal(seen_argv, argv);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_forwards_arguments),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
