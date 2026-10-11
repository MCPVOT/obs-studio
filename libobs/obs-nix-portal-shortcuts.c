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

#include "obs-nix-portal-shortcuts.h"

/* RED stub: no portal client, as on Wayland today. */

struct obs_nix_portal_shortcuts *obs_nix_portal_shortcuts_start(struct obs_nix_hotkey_state *state,
								obs_nix_portal_key_name_func key_name)
{
	UNUSED_PARAMETER(state);
	UNUSED_PARAMETER(key_name);
	return NULL;
}

void obs_nix_portal_shortcuts_stop(struct obs_nix_portal_shortcuts *portal)
{
	UNUSED_PARAMETER(portal);
}

bool obs_nix_portal_shortcut_id(obs_key_combination_t combo, struct dstr *id)
{
	UNUSED_PARAMETER(combo);
	UNUSED_PARAMETER(id);
	return false;
}

bool obs_nix_portal_shortcut_combo(const char *id, obs_key_combination_t *combo)
{
	UNUSED_PARAMETER(id);
	UNUSED_PARAMETER(combo);
	return false;
}

bool obs_nix_portal_shortcut_trigger(obs_key_combination_t combo, obs_nix_portal_key_name_func key_name,
				     struct dstr *trigger)
{
	UNUSED_PARAMETER(combo);
	UNUSED_PARAMETER(key_name);
	UNUSED_PARAMETER(trigger);
	return false;
}
