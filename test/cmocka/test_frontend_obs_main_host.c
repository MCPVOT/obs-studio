#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "frontend/utility/obs-main-macos.hpp"
#include "frontend/utility/obs-main-windows.hpp"

/*
 * The frontend's Windows-only logging and startup checks and its macOS
 * startup in obs-main (obs-rust/obs-studio#865) live in self-guarded headers:
 * on Linux they define nothing, so obs-main echoes log lines to the
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
#ifdef OBS_MAIN_HOST_MACOS
	defined |= 2;
#endif
#if defined(OBS_MAIN_HOST_EARLY_INIT) || defined(ALLOW_PORTABLE_MODE)
	defined |= 4;
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
