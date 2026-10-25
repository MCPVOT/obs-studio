/* "Use <key> in OBS" for a row the desktop runs on another key
 * (obs-rust/obs-studio#401): HotkeyBackground::DesktopCombo gives the
 * desktop's key as an OBS key combination when it can be read exactly, and
 * nothing when it can't (localized modifier names, Hyprland's config). The
 * trigger texts are the forms KDE Plasma 6 and GNOME 50 report (#791). */

/* Before cmocka.h, whose fail() macro would hit std::basic_ios::fail(). */
#include <fstream>

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
extern "C" {
#include <cmocka.h>
}

#include "OBSHotkeyBackground.hpp"

using namespace HotkeyBackground;

static const obs_key_combination_t f13 = {0, OBS_KEY_F13};
static const obs_key_combination_t ctrl_shift_a = {INTERACT_CONTROL_KEY | INTERACT_SHIFT_KEY, OBS_KEY_A};

static Input desktop_input(const char *f13_trigger, const char *a_trigger)
{
	Input input;
	input.state = OBS_HOTKEYS_BACKGROUND_ACTIVE;
	input.appId = "com.obsproject.Studio";
	input.flags = OBS_HOTKEYS_BACKGROUND_DESKTOP_TRIGGERS | OBS_HOTKEYS_BACKGROUND_CONFIGURABLE;
	input.combos = {
		{f13, "OBS_KEY_F13", "F13"},
		{ctrl_shift_a, "ctrl+shift+OBS_KEY_A", "CTRL+SHIFT+a"},
	};
	input.shortcuts = {{f13, f13_trigger, true}, {ctrl_shift_a, a_trigger, true}};
	return input;
}

static void assert_combo(std::optional<obs_key_combination_t> got, uint32_t modifiers, obs_key_t key)
{
	assert_true(got.has_value());
	assert_int_equal(got->modifiers, modifiers);
	assert_int_equal(got->key, key);
}

/* KDE moved Push-to-talk's F13 to Meta+P: OBS can take Meta+P. */
static void kde_other_key_is_offered(void **)
{
	Input input = desktop_input("Meta+P", "Ctrl+Shift+A");
	assert_int_equal((int)MarkerFor(input, f13), (int)Marker::OtherKey);
	assert_combo(DesktopCombo(input, f13), INTERACT_COMMAND_KEY, OBS_KEY_P);
	/* The row on the same key has nothing to offer. */
	assert_false(DesktopCombo(input, ctrl_shift_a).has_value());
}

/* GNOME's GTK accelerator after a word. */
static void gnome_other_key_is_offered(void **)
{
	Input input = desktop_input("F13", "Press <Shift><Control>b");
	assert_combo(DesktopCombo(input, ctrl_shift_a), INTERACT_CONTROL_KEY | INTERACT_SHIFT_KEY, OBS_KEY_B);
	assert_false(DesktopCombo(input, f13).has_value());
}

/* KDE in German: "Strg+Umschalt+B" is a different key (B), but its modifier
 * names aren't read, so OBS shows "other key" without offering a key. The
 * same key in German is never a mismatch. */
static void localized_modifiers_are_not_offered(void **)
{
	Input input = desktop_input("F13", "Strg+Umschalt+B");
	assert_int_equal((int)MarkerFor(input, ctrl_shift_a), (int)Marker::OtherKey);
	assert_false(DesktopCombo(input, ctrl_shift_a).has_value());

	input = desktop_input("F13", "Strg+Umschalt+A");
	assert_int_equal((int)MarkerFor(input, ctrl_shift_a), (int)Marker::Background);
	assert_false(DesktopCombo(input, ctrl_shift_a).has_value());
}

/* A shortcut the desktop hasn't bound has no key to take. */
static void unbound_shortcut_offers_nothing(void **)
{
	Input input = desktop_input("Meta+P", "Ctrl+Shift+A");
	input.shortcuts[0].bound = false;
	assert_false(DesktopCombo(input, f13).has_value());
}

/* Hyprland: the key lives in the user's config, which OBS doesn't rewrite. */
static void hyprland_offers_nothing(void **)
{
	Input input = desktop_input("Meta+P", "Ctrl+Shift+A");
	DesktopConfig desktop;
	desktop.name = "Hyprland";
	input.desktop = desktop;
	assert_false(DesktopCombo(input, f13).has_value());
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(kde_other_key_is_offered),
		cmocka_unit_test(gnome_other_key_is_offered),
		cmocka_unit_test(localized_modifiers_are_not_offered),
		cmocka_unit_test(unbound_shortcut_offers_nothing),
		cmocka_unit_test(hyprland_offers_nothing),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
