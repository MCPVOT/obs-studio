/* win-capture: Window Capture's "List OBS windows" option (#2226).
 * ms_fill_window_list and ms_find_window leave out the windows of the
 * current process; obs-windows.c.h lists and finds them when the option is
 * on. The test opens a window in its own process, as OBS's are, and checks
 * that the helpers list it and find it again, and that the libobs helpers
 * still leave it out. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <windows.h>

#include <obs.h>
#include <util/dstr.h>
#include <util/windows/window-helpers.h>

#include "plugins/win-capture/obs-windows.c.h"

#define TEST_CLASS L"ObsTestOwnWindow"
#define TEST_TITLE L"OBS test projector"

static HWND window;

static int setup(void **state)
{
	(void)state;

	WNDCLASSW wc = {0};
	wc.lpfnWndProc = DefWindowProcW;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.lpszClassName = TEST_CLASS;
	if (!RegisterClassW(&wc))
		return -1;

	window = CreateWindowExW(0, TEST_CLASS, TEST_TITLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, 320, 240, NULL,
				 NULL, wc.hInstance, NULL);
	return window ? 0 : -1;
}

static int teardown(void **state)
{
	(void)state;
	if (window)
		DestroyWindow(window);
	UnregisterClassW(TEST_CLASS, GetModuleHandleW(NULL));
	return 0;
}

static bool list_has(obs_property_t *p, const char *needle)
{
	for (size_t i = 0; i < obs_property_list_item_count(p); i++) {
		const char *val = obs_property_list_item_string(p, i);
		if (val && strstr(val, needle))
			return true;
	}
	return false;
}

static void own_windows_are_listed_only_by_the_option(void **state)
{
	(void)state;

	obs_properties_t *props = obs_properties_create();
	obs_property_t *p =
		obs_properties_add_list(props, "window", "Window", OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);

	ms_fill_window_list(p, EXCLUDE_MINIMIZED, NULL);
	assert_false(list_has(p, "ObsTestOwnWindow"));

	obs_windows_fill(p);
	assert_true(list_has(p, "OBS test projector:ObsTestOwnWindow:"));

	obs_properties_destroy(props);
}

static void own_window_is_found_again(void **state)
{
	(void)state;

	struct dstr exe = {0};
	obs_windows_exe(&exe);
	assert_non_null(exe.array);

	assert_null(ms_find_window(INCLUDE_MINIMIZED, WINDOW_PRIORITY_CLASS, "ObsTestOwnWindow", "OBS test projector",
				   exe.array));
	assert_ptr_equal(obs_windows_find(WINDOW_PRIORITY_CLASS, "ObsTestOwnWindow", "OBS test projector", exe.array),
			 window);
	assert_ptr_equal(obs_windows_find(WINDOW_PRIORITY_TITLE, "OtherClass", "OBS test projector", exe.array),
			 window);
	/* Another program's saved window is never matched to an OBS window. */
	assert_null(obs_windows_find(WINDOW_PRIORITY_CLASS, "ObsTestOwnWindow", "OBS test projector", "other.exe"));

	dstr_free(&exe);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(own_windows_are_listed_only_by_the_option),
		cmocka_unit_test(own_window_is_found_again),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
