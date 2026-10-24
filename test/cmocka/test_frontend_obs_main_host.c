#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "frontend/utility/obs-main-windows.hpp"

/*
 * The frontend's Windows-only logging in obs-main (debugger output and the
 * break on errors, obs-rust/obs-studio#865) lives in a self-guarded Windows
 * header: off Windows it defines nothing, so obs-main echoes log lines to the
 * console with its portable defaults. The Windows side is compiled by the
 * cross check.
 */

static void test_nothing_off_windows(void **state)
{
	(void)state;
	int defined = 0;
#ifdef OBS_MAIN_HOST_WINDOWS
	defined |= 1;
#endif
	assert_int_equal(defined, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_nothing_off_windows),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
