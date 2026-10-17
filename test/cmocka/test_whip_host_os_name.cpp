extern "C" {
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
}

#include "plugins/obs-webrtc/whip-host-posix.cpp.hpp"

/*
 * obs-webrtc's host interface (obs-rust/obs-studio#861): the POSIX host names
 * the operating system in the WHIP User-Agent the way the old compile-time
 * chain did, now from uname(): "OpenBSD", "FreeBSD", "Linux x86_64" on any
 * 64-bit Linux, "Linux" otherwise.
 */

static void test_linux_64bit_name(void **state)
{
	(void)state;
	struct utsname u;

	assert_int_equal(uname(&u), 0);
	if (strcmp(u.sysname, "Linux") != 0) {
		skip();
	}
	assert_int_equal(sizeof(void *), 8);
	assert_string_equal(whip_host_os_name(), "Linux x86_64");
}

static void test_matches_uname_family(void **state)
{
	(void)state;
	struct utsname u;
	const char *name = whip_host_os_name();

	assert_int_equal(uname(&u), 0);
	assert_non_null(name);
	if (strcmp(u.sysname, "OpenBSD") == 0 || strcmp(u.sysname, "FreeBSD") == 0) {
		assert_string_equal(name, u.sysname);
	} else {
		assert_int_equal(strncmp(name, "Linux", 5), 0);
	}
	/* A static string: the same pointer every call. */
	assert_ptr_equal(name, whip_host_os_name());
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_linux_64bit_name),
		cmocka_unit_test(test_matches_uname_family),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
