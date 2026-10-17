#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/frontend-tools/frontend-tools-host-macos.c.h"

/*
 * frontend-tools' macOS host (obs-rust/obs-studio#855), plain C so it builds
 * anywhere: the Python settings tab is offered, and a saved Python path is
 * cut before "/Python.framework/Versions" as InitScripts did inline.
 */

static void test_python_ui(void **state)
{
	(void)state;
	assert_true(frontend_tools_host_python_ui());
}

static void test_python_path_trimmed(void **state)
{
	(void)state;
	assert_int_equal(frontend_tools_host_python_path_len("/Library/Frameworks/Python.framework/Versions/3.11"),
			 strlen("/Library/Frameworks"));
	assert_int_equal(frontend_tools_host_python_path_len("/Python.framework/Versions"), 0);
	assert_int_equal(
		frontend_tools_host_python_path_len("/a/Python.framework/Versions/x/Python.framework/Versions"),
		strlen("/a"));
}

static void test_python_path_kept(void **state)
{
	(void)state;
	assert_int_equal(frontend_tools_host_python_path_len("/Library/Frameworks"), strlen("/Library/Frameworks"));
	assert_int_equal(frontend_tools_host_python_path_len("/a/Python.framework/3.11"),
			 strlen("/a/Python.framework/3.11"));
	assert_int_equal(frontend_tools_host_python_path_len(""), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_python_ui),
		cmocka_unit_test(test_python_path_trimmed),
		cmocka_unit_test(test_python_path_kept),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
