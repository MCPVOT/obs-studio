#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/obs-qsv11/qsv-host-linux.c.h"

/*
 * obs-qsv11's host interface (obs-rust/obs-studio#860): on Linux a texture
 * can be encoded when both plane textures are set; the shared handle (a
 * Windows thing) does not matter.
 */

static void test_null_texture(void **state)
{
	(void)state;
	assert_false(qsv_host_texture_valid(NULL));
}

static void test_both_planes(void **state)
{
	(void)state;
	struct encoder_texture tex = {0};

	tex.handle = (uint32_t)-1; /* GS_INVALID_HANDLE, Windows-only */
	tex.tex[0] = (struct gs_texture *)(uintptr_t)0x10;
	tex.tex[1] = (struct gs_texture *)(uintptr_t)0x20;
	assert_true(qsv_host_texture_valid(&tex));
}

static void test_missing_plane(void **state)
{
	(void)state;
	struct encoder_texture tex = {0};

	tex.handle = 1;
	tex.tex[0] = (struct gs_texture *)(uintptr_t)0x10;
	assert_false(qsv_host_texture_valid(&tex));

	tex.tex[0] = NULL;
	tex.tex[1] = (struct gs_texture *)(uintptr_t)0x20;
	assert_false(qsv_host_texture_valid(&tex));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_null_texture),
		cmocka_unit_test(test_both_planes),
		cmocka_unit_test(test_missing_plane),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
