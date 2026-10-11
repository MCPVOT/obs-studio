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

#include "obs-nix-hotkey-state.h"

/* RED stub: behaves like the Wayland platform today, which never reports a
 * key as pressed. */

void obs_nix_hotkey_state_set_key(struct obs_nix_hotkey_state *state, obs_key_t key, bool pressed)
{
	UNUSED_PARAMETER(state);
	UNUSED_PARAMETER(key);
	UNUSED_PARAMETER(pressed);
}

void obs_nix_hotkey_state_set_modifiers(struct obs_nix_hotkey_state *state, uint32_t modifiers)
{
	UNUSED_PARAMETER(state);
	UNUSED_PARAMETER(modifiers);
}

void obs_nix_hotkey_state_clear_keys(struct obs_nix_hotkey_state *state)
{
	UNUSED_PARAMETER(state);
}

bool obs_nix_hotkey_state_is_pressed(struct obs_nix_hotkey_state *state, obs_key_t key)
{
	UNUSED_PARAMETER(state);
	UNUSED_PARAMETER(key);
	return false;
}
