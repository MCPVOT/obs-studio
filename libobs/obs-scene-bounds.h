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

#include "obs.h"
#include "graphics/vec2.h"

/* Scales a source of cx x cy pixels, drawn at `scale`, so that it fits a
 * bounding box of `bounds` pixels the way `type` says (any OBS_BOUNDS_*
 * other than OBS_BOUNDS_NONE). Flips (negative scales) are kept.
 *
 * This is the scale half of the scene item transform (calculate_bounds_data
 * in obs-scene.c), kept free of scene items so it can be tested on its own. */
void obs_bounds_fit_scale(enum obs_bounds_type type, const struct vec2 *bounds, uint32_t cx, uint32_t cy,
			  struct vec2 *scale);

/* The rest of that transform, once the scene has resolved `bounds` to pixels.
 * Fits the scale, aligns `origin` in the box, optionally crops, and keeps a
 * flipped item in place. `cx` and `cy` come in as the source size and go out
 * as the box size. The scene passes an origin of 0, 0. */
void obs_bounds_place(enum obs_bounds_type type, const struct vec2 *bounds, struct vec2 *origin, struct vec2 *scale,
		      uint32_t *cx, uint32_t *cy, uint32_t align, bool crop_to_bounds, struct obs_sceneitem_crop *crop);
