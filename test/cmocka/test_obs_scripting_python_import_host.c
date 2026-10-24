#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include OBS_PYTHON_IMPORT_HOST_SOURCE

/*
 * The Python import hosts (obs-rust/obs-studio#882): the library file each
 * one looks for in the Python install folder, as VERSION_PATTERN and
 * FILE_PATTERN spelled it inline. Both host files are plain C, so they build
 * and run here; OBS_PYTHON_IMPORT_HOST_SOURCE picks one per executable.
 */

static void expect(int major, int minor, const char *name)
{
	struct dstr out = {0};
	obs_python_host_lib_name(&out, major, minor);
	assert_non_null(out.array);
	assert_string_equal(out.array, name);
	dstr_free(&out);
}

static void test_names(void **state)
{
	(void)state;
#if defined(TEST_HOST_WINDOWS)
	expect(3, 12, "python312.dll");
	expect(3, 6, "python36.dll");
#else
	expect(3, 12, "Python.framework/Versions/3.12/lib/libpython3.12.dylib");
	expect(3, 6, "Python.framework/Versions/3.6/lib/libpython3.6.dylib");
#endif
}

/* A reused string is replaced, not appended to. */
static void test_reuse(void **state)
{
	(void)state;
	struct dstr out = {0};
	obs_python_host_lib_name(&out, 3, 12);
	obs_python_host_lib_name(&out, 3, 11);
	assert_non_null(out.array);
	assert_null(strstr(out.array, "12"));
	dstr_free(&out);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_names),
		cmocka_unit_test(test_reuse),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
