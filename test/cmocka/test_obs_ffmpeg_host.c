#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include OBS_FFMPEG_HOST_SOURCE

/*
 * obs-ffmpeg's Linux and POSIX hosts (obs-rust/obs-studio#853). Without the
 * NVIDIA encode library NVENC is unavailable, whatever devices the machine
 * has; module load and unload have no host work. OBS_FFMPEG_HOST_SOURCE
 * picks the host. The Windows host (DXGI, AMF) is compiled by the cross
 * check.
 */

static bool never_blacklisted(int device_id)
{
	(void)device_id;
	return false;
}

static void test_nvenc_needs_the_encode_library(void **state)
{
	(void)state;
	void *lib = os_dlopen("libnvidia-encode.so.1");
	if (lib) {
		os_dlclose(lib);
		skip();
	}

	assert_false(obs_ffmpeg_host_nvenc_available(never_blacklisted));
}

static void test_load_unload(void **state)
{
	(void)state;
	obs_ffmpeg_host_load();
	obs_ffmpeg_host_unload();
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_nvenc_needs_the_encode_library),
		cmocka_unit_test(test_load_unload),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
