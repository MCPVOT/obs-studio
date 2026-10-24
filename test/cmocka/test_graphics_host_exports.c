#include <string.h>
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "libobs/graphics/graphics-internal.h"

/*
 * libobs/graphics' per-OS export lists (obs-rust/obs-studio#872): on Linux
 * the device's dmabuf, pixmap and sync entry points stay in struct
 * gs_exports, and graphics-imports-linux.h imports exactly those eleven,
 * all required. The macOS and Windows lists are compiled by the cross check.
 */

static const char *imported[16];
static size_t num_imported;
static size_t num_optional;

#define GRAPHICS_IMPORT(func) imported[num_imported++] = #func
#define GRAPHICS_IMPORT_OPTIONAL(func) num_optional++

static void import_linux(void)
{
#include "libobs/graphics/graphics-imports-linux.h"
}

static void test_linux_imports(void **state)
{
	(void)state;
	import_linux();

	assert_int_equal(num_imported, 11);
	assert_int_equal(num_optional, 0);
	assert_string_equal(imported[0], "device_texture_create_from_dmabuf");
	assert_string_equal(imported[3], "device_texture_create_from_pixmap");
	assert_string_equal(imported[10], "device_sync_wait");
}

static void test_linux_exports(void **state)
{
	(void)state;
	struct gs_exports exports = {0};

	assert_null(exports.device_texture_create_from_dmabuf);
	assert_null(exports.device_sync_wait);
	assert_true(offsetof(struct gs_exports, device_sync_wait) >
		    offsetof(struct gs_exports, device_texture_create_from_dmabuf));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_linux_imports),
		cmocka_unit_test(test_linux_exports),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
