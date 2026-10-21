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

#include "graphics/vec2.h"

/* Scene item coordinates, between the relative form stored on the item and
 * pixels. The scene still reads the canvas (get_scene_dimensions) and still
 * decides the two flags. This is only the math, so a test can call it
 * without a scene.
 *
 * Relative position treats the canvas height as [-1, 1], centered. Relative
 * size treats that same height as 2. `canvas_x` and `canvas_y` are pixels.
 * `nudge` snaps the pixel result onto a half pixel. A group item, or an item
 * whose parent is a group, passes false. Group and nested-scene scales are
 * not converted: the scene returns before these scale functions. */

/* True unless the item is a group or its parent is a group. */
bool obs_scene_coords_nudge(bool is_group, bool parent_is_group);

/* True for a group item or a nested scene. The scene then keeps the scale
 * it already has and does not call the scale functions below. */
bool obs_scene_coords_keep_scale(bool is_group, bool is_scene);

/* floor(v * 2 + 0.5) / 2. Half pixels stay. Other values move to a half. */
void obs_scene_nudge_abs(struct vec2 *dst, const struct vec2 *v);

void obs_scene_pos_to_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_x, float canvas_y, bool nudge);
void obs_scene_pos_from_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_x, float canvas_y);

void obs_scene_size_to_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_y, bool nudge);
void obs_scene_size_from_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_y);

/* scale * (canvas_y / scale_ref_y). The scene does not call this when
 * obs_scene_coords_keep_scale is true. */
void obs_scene_item_canvas_scale(struct vec2 *dst, const struct vec2 *scale, float canvas_y, float scale_ref_y);

/* v * (scale_ref_y / canvas_y). The inverse of the canvas scale. */
void obs_scene_item_relative_scale(struct vec2 *dst, const struct vec2 *v, float canvas_y, float scale_ref_y);
