// The C++ headers go first: cmocka.h defines macros (fail, ...) that clash
// with libstdc++.
#include <filesystem>
#include <fstream>
#include <string>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>

#include <util/base.h>
#include <util/c99defs.h>

#include "scene-collection-migration.hpp"

namespace fs = std::filesystem;

/* obs-rust/obs-studio#155: every fresh profile logged
 * "Failed to rename basic scene collection file" because the migration
 * renamed a basic/scenes.json that never existed. */

static int errors_logged;

static void count_errors(int level, const char *format, va_list args, void *param)
{
	UNUSED_PARAMETER(format);
	UNUSED_PARAMETER(args);
	UNUSED_PARAMETER(param);
	if (level == LOG_ERROR) {
		errors_logged++;
	}
}

static fs::path root()
{
	return fs::current_path() / "scene_collection_migration_test";
}

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	fs::remove_all(root());
	fs::create_directories(root() / "basic");
	errors_logged = 0;
	base_set_log_handler(count_errors, nullptr);
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	base_set_log_handler(nullptr, nullptr);
	fs::remove_all(root());
	return 0;
}

static void fresh_profile_logs_no_error(void **state)
{
	UNUSED_PARAMETER(state);
	migrate_basic_scene_collection(root() / "basic", root() / "basic" / "scenes", "Untitled");

	assert_int_equal(errors_logged, 0);
	assert_false(fs::exists(root() / "basic" / "scenes" / "Untitled.json"));
}

static void legacy_scenes_file_is_moved(void **state)
{
	UNUSED_PARAMETER(state);
	std::ofstream(root() / "basic" / "scenes.json") << "{}";

	migrate_basic_scene_collection(root() / "basic", root() / "basic" / "scenes", "Untitled");

	assert_int_equal(errors_logged, 0);
	assert_false(fs::exists(root() / "basic" / "scenes.json"));
	assert_true(fs::exists(root() / "basic" / "scenes" / "Untitled.json"));
}

static void existing_collections_are_left_alone(void **state)
{
	UNUSED_PARAMETER(state);
	std::ofstream(root() / "basic" / "scenes.json") << "{}";
	fs::create_directories(root() / "basic" / "scenes");

	migrate_basic_scene_collection(root() / "basic", root() / "basic" / "scenes", "Untitled");

	assert_int_equal(errors_logged, 0);
	assert_true(fs::exists(root() / "basic" / "scenes.json"));
	assert_false(fs::exists(root() / "basic" / "scenes" / "Untitled.json"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(fresh_profile_logs_no_error, setup, teardown),
		cmocka_unit_test_setup_teardown(legacy_scenes_file_is_moved, setup, teardown),
		cmocka_unit_test_setup_teardown(existing_collections_are_left_alone, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
