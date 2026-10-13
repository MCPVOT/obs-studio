// The C++ headers go first: cmocka.h defines macros (fail, ...) that clash
// with libstdc++.
#include <filesystem>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
// The Windows cmocka.h has no extern "C" guard of its own.
extern "C" {
#include <cmocka.h>
}

#include <util/c99defs.h>

#include "crash-sentinel-dir.hpp"

namespace fs = std::filesystem;

/* obs-rust/obs-studio#154: CrashHandler runs before MakeUserDirs, so on a
 * first launch the sentinel directory's parent (the obs-studio config
 * directory) doesn't exist yet, and the first session ran with no sentinel. */

static fs::path root()
{
	return fs::current_path() / "crash_sentinel_dir_test";
}

static int setup(void **state)
{
	UNUSED_PARAMETER(state);
	fs::remove_all(root());
	fs::create_directories(root());
	return 0;
}

static int teardown(void **state)
{
	UNUSED_PARAMETER(state);
	fs::remove_all(root());
	return 0;
}

static void missing_parent_is_created(void **state)
{
	UNUSED_PARAMETER(state);
	const fs::path sentinel = root() / "obs-studio" / ".sentinel";

	assert_true(ensure_crash_sentinel_dir(sentinel));
	assert_true(fs::is_directory(sentinel));
}

static void existing_directory_is_kept(void **state)
{
	UNUSED_PARAMETER(state);
	const fs::path sentinel = root() / ".sentinel";
	fs::create_directories(sentinel);

	assert_true(ensure_crash_sentinel_dir(sentinel));
	assert_true(fs::is_directory(sentinel));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup_teardown(missing_parent_is_created, setup, teardown),
		cmocka_unit_test_setup_teardown(existing_directory_is_kept, setup, teardown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
