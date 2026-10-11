/******************************************************************************
    Copyright (C) 2026 by the obs-rust contributors

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
******************************************************************************/

#pragma once

#include <stdbool.h>
#include <xcb/xcb.h>

/* Whether keycode `code` is down in a QueryKeymap reply. */
static inline bool obs_nix_x11_keycode_pressed(const xcb_query_keymap_reply_t *reply, xcb_keycode_t code)
{
	return (reply->keys[code / 8] & (1 << (code % 8))) != 0;
}
