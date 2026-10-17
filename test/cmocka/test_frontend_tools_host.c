#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/frontend-tools/frontend-tools-host-posix.c.h"

/*
 * frontend-tools' host interface on POSIX hosts other than macOS
 * (obs-rust/obs-studio#855): no Python settings tab, and a saved Python path
 * is used as is, even one under a Python.framework.
 */

static void test_no_python_ui(void **state)
{
	(void)state;
	assert_false(frontend_tools_host_python_ui());
}

static void test_python_path_kept(void **state)
{
	(void)state;
	const char *framework = "/Library/Frameworks/Python.framework/Versions/3.11";
	assert_int_equal(frontend_tools_host_python_path_len(framework), strlen(framework));
	assert_int_equal(frontend_tools_host_python_path_len("/usr/lib/python3.11"), strlen("/usr/lib/python3.11"));
	assert_int_equal(frontend_tools_host_python_path_len(""), 0);
}

static void test_load_unload(void **state)
{
	(void)state;
	frontend_tools_host_load();
	frontend_tools_host_unload();
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_no_python_ui),
		cmocka_unit_test(test_python_path_kept),
		cmocka_unit_test(test_load_unload),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
