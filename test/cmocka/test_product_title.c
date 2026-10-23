/* The main window's product title (#1983): "OBS Studio Rust <version>", with
 * the release version when the build has one and otherwise a dev version made
 * from the configure-time `git describe` of the newest obs-studio-rust-v* tag.
 * Built without Qt, from frontend/utility/product-name.c. */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <string.h>
#include <cmocka.h>

#include "product-name.h"

static void check(const char *release, const char *describe, const char *want)
{
	char buf[128];

	assert_int_equal(obs_product_title(buf, sizeof(buf), release, describe), (int)strlen(want));
	assert_string_equal(buf, want);
}

static void release_build_shows_release_version(void **state)
{
	(void)state;
	check("0.1.0-beta.20261023", "obs-studio-rust-v0.1.0-beta.20261022-3-gabc1234",
	      "OBS Studio Rust 0.1.0-beta.20261023");
	check("0.1.0-beta.20261023", NULL, "OBS Studio Rust 0.1.0-beta.20261023");
}

static void dev_build_shows_newest_tag_with_dev_suffix(void **state)
{
	(void)state;
	check(NULL, "obs-studio-rust-v0.1.0-beta.20261023-78-g4527fbf78",
	      "OBS Studio Rust 0.1.0-beta.20261023-dev+78-g4527fbf78");
	/* An empty release version is no release version. */
	check("", "obs-studio-rust-v0.1.0-beta.20261023-78-g4527fbf78",
	      "OBS Studio Rust 0.1.0-beta.20261023-dev+78-g4527fbf78");
	/* Built exactly at the tag: still a dev build. */
	check(NULL, "obs-studio-rust-v0.2.0-0-g0123abc", "OBS Studio Rust 0.2.0-dev+0-g0123abc");
}

static void dev_build_without_tag_is_dev(void **state)
{
	(void)state;
	check(NULL, NULL, "OBS Studio Rust dev");
	check(NULL, "", "OBS Studio Rust dev");
	/* A shallow clone's describe failure, or another tag scheme. */
	check(NULL, "4527fbf78", "OBS Studio Rust dev");
	check(NULL, "v33.0.0-beta6-1902-g582a4c3ed", "OBS Studio Rust dev");
	check(NULL, "obs-studio-rust-v0.1.0", "OBS Studio Rust dev");
	check(NULL, "obs-studio-rust-v0.1.0-x-gabc", "OBS Studio Rust dev");
	check(NULL, "obs-studio-rust-v-3-gabc", "OBS Studio Rust dev");
}

static void truncates_and_terminates(void **state)
{
	(void)state;
	char buf[8];

	assert_int_equal(obs_product_title(buf, sizeof(buf), "0.3.0", NULL), (int)strlen("OBS Studio Rust 0.3.0"));
	assert_string_equal(buf, "OBS Stu");
	assert_int_equal(obs_product_title(NULL, 0, "0.3.0", NULL), (int)strlen("OBS Studio Rust 0.3.0"));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(release_build_shows_release_version),
		cmocka_unit_test(dev_build_shows_newest_tag_with_dev_suffix),
		cmocka_unit_test(dev_build_without_tag_is_dev),
		cmocka_unit_test(truncates_and_terminates),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
