#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include OBS_PYTHON_HOST_SOURCE

/*
 * The Python scripting hosts (obs-rust/obs-studio#882): whether Python loads
 * at start, PYTHONHOME, bytecode, and the sys.path directories, as
 * obs_scripting_load() and obs_scripting_load_python() chose them inline.
 * Each host file is plain C, so all three build and run here;
 * OBS_PYTHON_HOST_SOURCE picks one per executable (tests.d).
 */

#define MAX_DIRS 4

static char *dirs[MAX_DIRS];
static size_t ndirs;

static void record(const char *dir)
{
	assert_non_null(dir);
	assert_true(ndirs < MAX_DIRS);
	dirs[ndirs++] = bstrdup(dir);
}

static void collect(void)
{
	ndirs = 0;
	obs_python_host_module_dirs(record);
}

static void release(void)
{
	for (size_t i = 0; i < ndirs; i++)
		bfree(dirs[i]);
	ndirs = 0;
}

static void expect_home(const char *python_path, int major, int minor, const char *home)
{
	struct dstr out = {0};
	obs_python_host_home(&out, python_path, major, minor);
	assert_non_null(out.array);
	assert_string_equal(out.array, home);
	dstr_free(&out);
}

#if defined(TEST_HOST_MACOS)
/* macOS: Python waits for the user's install folder, whose framework holds
 * PYTHONHOME; no bytecode in the bundle; the bundle's PlugIns and Resources
 * folders, when they exist. */
static void test_host(void **state)
{
	(void)state;
	assert_false(obs_python_host_load_at_start());
	assert_true(obs_python_host_no_bytecode());
	expect_home("/Library/Frameworks", 3, 11, "/Library/Frameworks/Python.framework/Versions/3.11");

	char *plugins = os_get_executable_path_ptr("../PlugIns");
	char *resources = os_get_executable_path_ptr("../Resources");
	os_rmdir(plugins);
	os_rmdir(resources);
	collect();
	assert_int_equal(ndirs, 0);

	assert_int_not_equal(os_mkdir(plugins), MKDIR_ERROR);
	assert_int_not_equal(os_mkdir(resources), MKDIR_ERROR);
	char *abs_plugins = os_get_abs_path_ptr(plugins);
	char *abs_resources = os_get_abs_path_ptr(resources);
	collect();
	assert_int_equal(ndirs, 2);
	assert_string_equal(dirs[0], abs_plugins);
	assert_string_equal(dirs[1], abs_resources);
	release();

	os_rmdir(plugins);
	os_rmdir(resources);
	bfree(abs_plugins);
	bfree(abs_resources);
	bfree(plugins);
	bfree(resources);
}
#else
/* Windows and the other POSIX hosts: PYTHONHOME is the install folder;
 * SCRIPT_DIR relative to the executable's folder, then SCRIPT_DIR as an
 * absolute path when it exists. Only the POSIX host loads Python at start. */
static void test_host(void **state)
{
	(void)state;
#if defined(TEST_HOST_WINDOWS)
	assert_false(obs_python_host_load_at_start());
#else
	assert_true(obs_python_host_load_at_start());
#endif
	assert_false(obs_python_host_no_bytecode());
	expect_home("C:/Python311", 3, 11, "C:/Python311");

	char *relative = os_get_executable_path_ptr("../" SCRIPT_DIR);
	os_rmdir(SCRIPT_DIR);
	collect();
	assert_int_equal(ndirs, 1);
	assert_string_equal(dirs[0], relative);
	release();

	assert_int_not_equal(os_mkdir(SCRIPT_DIR), MKDIR_ERROR);
	char *absolute = os_get_abs_path_ptr(SCRIPT_DIR);
	collect();
	assert_int_equal(ndirs, 2);
	assert_string_equal(dirs[0], relative);
	assert_string_equal(dirs[1], absolute);
	release();

	os_rmdir(SCRIPT_DIR);
	bfree(absolute);
	bfree(relative);
}
#endif

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_host),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
