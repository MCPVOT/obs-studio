#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "frontend/settings/settings-linux.hpp"
#include "frontend/settings/settings-macos.hpp"
#include "frontend/settings/settings-windows.hpp"

/*
 * The Settings dialog's platform options (obs-rust/obs-studio#867) come from
 * self-guarded host headers that name features instead of platforms. On
 * Linux only browser hardware acceleration is available: no Windows advanced
 * options, renderer choice, macOS vsync, always-on-top projector update or
 * filename filter.
 */

static void test_linux_features(void **state)
{
	(void)state;
	int defined = 0;
#ifdef OBS_SETTINGS_WINDOWS
	defined |= 1;
#endif
#ifdef OBS_SETTINGS_RENDERER
	defined |= 2;
#endif
#ifdef OBS_SETTINGS_MACOS_VSYNC
	defined |= 4;
#endif
#ifdef OBS_SETTINGS_PROJECTOR_ON_TOP
	defined |= 8;
#endif
#ifdef OBS_SETTINGS_FILENAME_UNSAFE
	defined |= 16;
#endif
	assert_int_equal(defined, 0);

	int hw_accel = 0;
#ifdef OBS_SETTINGS_BROWSER_HWACCEL
	hw_accel = 1;
#endif
	assert_int_equal(hw_accel, 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_linux_features),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
