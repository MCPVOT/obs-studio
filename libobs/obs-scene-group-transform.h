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

#include "graphics/matrix4.h"
#include "graphics/vec2.h"

/* Group transform math from obs-scene.c.h. The scene still reads the item.
 * It still converts absolute and relative coordinates, because that needs
 * the canvas. It still inverts the group's draw transform when an item
 * joins a group, and it still calls update_item_transform afterwards.
 *
 * obs_scene_transform_val maps a point through the matrix, translation
 * included. resize_group uses that to measure where a group sits.
 *
 * obs_scene_group_transform folds `transform` into `pos` and `scale`, then
 * adds `rot_delta` to `rot`. Leaving a group passes the draw transform and
 * the group's rotation. Joining one passes the inverse and the negated
 * rotation. The translation moves `pos` only. Scale is the length of the
 * x and y columns after the child's scale is multiplied by the matrix with
 * its translation cleared, and the sign of each axis is kept. A scale that
 * is not greater than zero takes the minus sign, so a zero scale becomes
 * negative zero. A rotation in the matrix does not change the scale. */

void obs_scene_transform_val(struct vec2 *dst, const struct matrix4 *transform);

void obs_scene_group_transform(const struct matrix4 *transform, struct vec2 *pos, struct vec2 *scale, float *rot,
			       float rot_delta);
