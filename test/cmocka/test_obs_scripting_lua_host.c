#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/bmem.h>
#include <util/platform.h>

#include OBS_LUA_HOST_SOURCE

/*
 * The Lua scripting hosts (obs-rust/obs-studio#882): the directories each one
 * adds to package.cpath before SCRIPT_DIR, as obs_lua_load() built them
 * inline. Each host file is plain C, so all three build and run here;
 * OBS_LUA_HOST_SOURCE picks one per executable (tests.d).
 */

#define MAX_DIRS 4

static char *dirs[MAX_DIRS];
static size_t ndirs;

static void record(struct dstr *cpath, const char *dir)
{
	assert_non_null(cpath);
	assert_true(ndirs < MAX_DIRS);
	dirs[ndirs++] = bstrdup(dir);
}

static void collect(void)
{
	struct dstr cpath = {0};
	ndirs = 0;
	obs_lua_host_module_dirs(&cpath, record);
	dstr_free(&cpath);
}

static void release(void)
{
	for (size_t i = 0; i < ndirs; i++)
		bfree(dirs[i]);
	ndirs = 0;
}

#if defined(TEST_HOST_WINDOWS)
/* Windows: only the working directory. */
static void test_dirs(void **state)
{
	(void)state;
	collect();
	assert_int_equal(ndirs, 1);
	assert_string_equal(dirs[0], "./");
	release();
}
#elif defined(TEST_HOST_MACOS)
/* macOS: the bundle's PlugIns folder next to the executable, as an absolute
 * path; nothing when there is none. */
static void test_dirs(void **state)
{
	(void)state;
	char *plugins = os_get_executable_path_ptr("../PlugIns");
	assert_non_null(plugins);
	os_rmdir(plugins);
	collect();
	assert_int_equal(ndirs, 0);

	assert_int_not_equal(os_mkdir(plugins), MKDIR_ERROR);
	char *absolute = os_get_abs_path_ptr(plugins);
	collect();
	assert_int_equal(ndirs, 1);
	assert_string_equal(dirs[0], absolute);
	release();
	os_rmdir(plugins);
	bfree(absolute);
	bfree(plugins);
}
#else
/* Other POSIX hosts: the working directory, then SCRIPT_DIR relative to the
 * executable's folder. */
static void test_dirs(void **state)
{
	(void)state;
	char *relative = os_get_executable_path_ptr("../" SCRIPT_DIR);
	collect();
	assert_int_equal(ndirs, 2);
	assert_string_equal(dirs[0], "./");
	assert_string_equal(dirs[1], relative);
	release();
	bfree(relative);
}
#endif

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_dirs),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
