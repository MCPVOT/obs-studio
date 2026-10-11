/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "obs-internal.h"

/* Key state for the libobs hotkey thread on Wayland.
 *
 * The hotkey thread polls obs_hotkeys_platform_is_pressed() every 25 ms and
 * releases any binding whose key is not reported as pressed. Wayland gives a
 * client no way to query the keyboard, so this state is fed from the two
 * sources that do exist:
 *
 *  - keys held while an OBS window has keyboard focus (wl_keyboard events),
 *  - global shortcuts that the xdg-desktop-portal GlobalShortcuts interface
 *    reports as activated, one per OBS key combination.
 *
 * Every function locks obs->hotkeys.mutex, the lock the hotkey thread holds
 * for a whole poll cycle, so a change never lands in the middle of a cycle.
 * obs_nix_hotkey_state_is_pressed() is the only function called with that
 * lock already held (by the hotkey thread; the mutex is recursive). */

struct obs_nix_hotkey_shortcut {
	obs_key_combination_t combo;
	/* A hotkey poll cycle has run while this entry was present. */
	bool observed;
	/* The portal already reported the shortcut as released; the entry is
	 * kept until a poll cycle has seen it so that a short tap still
	 * presses the hotkey. */
	bool release_pending;
};

struct obs_nix_hotkey_state {
	bool focused_keys[OBS_KEY_LAST_VALUE];
	uint32_t focused_modifiers;
	DARRAY(struct obs_nix_hotkey_shortcut) shortcuts;
};

void obs_nix_hotkey_state_free(struct obs_nix_hotkey_state *state);

/* Keyboard focus events. modifiers uses the INTERACT_*_KEY bits. */
void obs_nix_hotkey_state_set_key(struct obs_nix_hotkey_state *state, obs_key_t key, bool pressed);
void obs_nix_hotkey_state_set_modifiers(struct obs_nix_hotkey_state *state, uint32_t modifiers);
void obs_nix_hotkey_state_clear_keys(struct obs_nix_hotkey_state *state);

/* GlobalShortcuts Activated (active) / Deactivated (!active). */
void obs_nix_hotkey_state_set_shortcut(struct obs_nix_hotkey_state *state, obs_key_combination_t combo, bool active);

/* Drops released shortcuts that a poll cycle has seen. Returns true while a
 * released shortcut is still waiting to be seen; call again shortly. */
bool obs_nix_hotkey_state_flush(struct obs_nix_hotkey_state *state);

/* The answer for obs_hotkeys_platform_is_pressed(). */
bool obs_nix_hotkey_state_is_pressed(struct obs_nix_hotkey_state *state, obs_key_t key);
