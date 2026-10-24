#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include "plugins/obs-outputs/obs-outputs-host-posix.c.h"
#include "plugins/obs-outputs/utils.h"

/*
 * obs-outputs' module host (obs-rust/obs-studio#851): the POSIX host has no
 * load or unload work (Windows starts Winsock and mbedtls threading, which
 * the cross check compiles), and the bit helpers every compiler now shares
 * count leading and trailing zeros correctly.
 */

static void test_load_unload(void **state)
{
	(void)state;
	obs_outputs_host_load();
	obs_outputs_host_unload();
}

static void test_bit_counts(void **state)
{
	(void)state;
	assert_int_equal(clz32(1), 31);
	assert_int_equal(clz32(0x80000000u), 0);
	assert_int_equal(clz32(0x00010000u), 15);
	assert_int_equal(ctz32(1), 0);
	assert_int_equal(ctz32(0x80000000u), 31);
	assert_int_equal(ctz32(0x00000100u), 8);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_load_unload),
		cmocka_unit_test(test_bit_counts),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
