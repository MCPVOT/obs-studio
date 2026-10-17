#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/vlc-video/vlc-video-host-posix.c.h"

/*
 * vlc-video's host interface (obs-rust/obs-studio#857): on POSIX hosts the
 * VLC source hands libVLC its playlist entries exactly as entered, local
 * paths and URLs alike. Only the Windows host rewrites separators.
 */

static void check_unchanged(const char *in, bool is_url)
{
	struct dstr path = {0};

	dstr_copy(&path, in);
	vlc_host_native_path(&path, is_url);
	assert_non_null(path.array);
	assert_string_equal(path.array, in);
	dstr_free(&path);
}

static void test_local_path_unchanged(void **state)
{
	UNUSED_PARAMETER(state);
	check_unchanged("/home/user/videos/clip one.mkv", false);
	check_unchanged("relative/dir/clip.mp4", false);
	check_unchanged("C:/Users/me/clip.mp4", false);
}

static void test_url_unchanged(void **state)
{
	UNUSED_PARAMETER(state);
	check_unchanged("http://example.com/a/b/stream.m3u8", true);
	check_unchanged("file:///home/user/clip.mkv", true);
}

static void test_unload_is_safe_without_load(void **state)
{
	UNUSED_PARAMETER(state);
	vlc_host_unload();
	vlc_host_unload();
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_local_path_unchanged),
		cmocka_unit_test(test_url_unchanged),
		cmocka_unit_test(test_unload_is_safe_without_load),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
