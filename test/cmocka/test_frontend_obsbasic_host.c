#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "frontend/widgets/obsbasic-linux.hpp"
#include "frontend/widgets/obsbasic-macos.hpp"
#include "frontend/widgets/obsbasic-windows.hpp"

/*
 * The main window's platform behavior (obs-rust/obs-studio#871) comes from
 * self-guarded host headers that name features instead of platforms. On
 * Linux only the Ctrl+Q quit shortcut is on: no Windows or macOS window
 * setup, updater, crash-log menu, process priority or macOS tray masks.
 */

static void test_linux_features(void **state)
{
	(void)state;
	int defined = 0;
#ifdef OBS_BASIC_WINDOWS
	defined |= 1;
#endif
#ifdef OBS_BASIC_MACOS
	defined |= 2;
#endif
#ifdef OBS_BASIC_UPDATER
	defined |= 4;
#endif
#ifdef OBS_BASIC_CRASH_LOGS
	defined |= 8;
#endif
#ifdef OBS_BASIC_PROCESS_PRIORITY
	defined |= 16;
#endif
#ifdef OBS_BASIC_TRAY_ACTIVE
	defined |= 32;
#endif
	assert_int_equal(defined, 0);

	int quit_shortcut = 0;
#ifdef OBS_BASIC_QUIT_SHORTCUT
	quit_shortcut = 1;
#endif
	assert_int_equal(quit_shortcut, 1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_linux_features),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
