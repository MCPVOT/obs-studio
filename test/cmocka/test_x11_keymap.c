/* The X11 hotkey thread polls xcb_query_keymap. When the X connection is
 * closing or broken, xcb returns a NULL reply with no error, and reading the
 * key bits from it crashed the hotkey thread (obs-rust/obs-studio#126). */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include <string.h>

#include "obs-nix-x11-keymap.h"

static void null_reply_is_not_pressed(void **state)
{
	(void)state;
	assert_false(obs_nix_x11_keycode_pressed(NULL, 38));
	assert_false(obs_nix_x11_keycode_pressed(NULL, 255));
}

static void reply_bits_are_read(void **state)
{
	(void)state;
	xcb_query_keymap_reply_t reply;
	memset(&reply, 0, sizeof(reply));
	reply.keys[38 / 8] = (uint8_t)(1 << (38 % 8));
	assert_true(obs_nix_x11_keycode_pressed(&reply, 38));
	assert_false(obs_nix_x11_keycode_pressed(&reply, 39));
	assert_false(obs_nix_x11_keycode_pressed(&reply, 255));
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(null_reply_is_not_pressed),
		cmocka_unit_test(reply_bits_are_read),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
