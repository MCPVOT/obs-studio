#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

/*
 * The hotkey name table's macOS keypad list (obs-rust/obs-studio#870) is
 * its own header, guarded by __APPLE__: off macOS including it adds no
 * translation and leaves OBS_HOTKEY_NUMPAD_HOST undefined, so the shared
 * numpad names apply. The macOS list is compiled by the cross check.
 */

static int added;

#define ADD_TRANSLATION(key_name, var_name) added++

static void include_macos_keypad(void)
{
	struct {
		const char *apple_keypad_num;
	} t = {0};
	const char *numpad_str = NULL;
#include "libobs/obs-hotkey-translations-macos.h"
	(void)t;
	(void)numpad_str;
}

static void test_no_macos_keypad_here(void **state)
{
	(void)state;
	include_macos_keypad();
	assert_int_equal(added, 0);

	int host_numpad = 0;
#ifdef OBS_HOTKEY_NUMPAD_HOST
	host_numpad = 1;
#endif
	assert_int_equal(host_numpad, 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_no_macos_keypad_here),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
