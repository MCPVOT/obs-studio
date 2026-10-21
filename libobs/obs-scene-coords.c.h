/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>
                          Philippe Groarke <philippe.groarke@gmail.com>

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

#include "libobs/build/libobs-scene-coords.pch.h"

bool obs_scene_coords_nudge(bool is_group, bool parent_is_group)
{
	return !is_group && !parent_is_group;
}

bool obs_scene_coords_keep_scale(bool is_group, bool is_scene)
{
	return is_group || is_scene;
}

/* Rounds absolute pixel values to next .5. */
void obs_scene_nudge_abs(struct vec2 *dst, const struct vec2 *v)
{
	dst->x = floorf(v->x * 2.0f + 0.5f) / 2.0f;
	dst->y = floorf(v->y * 2.0f + 0.5f) / 2.0f;
}

void obs_scene_pos_from_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_x, float canvas_y)
{
	/* Scaled so that height (y) is [-1, 1]. */
	dst->x = (2 * v->x - canvas_x) / canvas_y;
	dst->y = 2 * v->y / canvas_y - 1.0f;
}

void obs_scene_pos_to_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_x, float canvas_y, bool nudge)
{
	dst->x = (v->x * canvas_y + canvas_x) / 2;
	dst->y = (v->y * canvas_y + canvas_y) / 2;

	/* In order for group resizing to behave properly they need all the
	 * precision they can get, so do not nudge their values. */
	if (nudge)
		obs_scene_nudge_abs(dst, dst);
}

void obs_scene_size_from_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_y)
{
	/* The height of the canvas is from [-1, 1], so 2.0f * aspect is the
	 * full width (depending on aspect ratio). */
	dst->x = (2 * v->x) / canvas_y;
	dst->y = 2 * v->y / canvas_y;
}

void obs_scene_size_to_absolute(struct vec2 *dst, const struct vec2 *v, float canvas_y, bool nudge)
{
	dst->x = (v->x * canvas_y) / 2;
	dst->y = (v->y * canvas_y) / 2;

	if (nudge)
		obs_scene_nudge_abs(dst, dst);
}

void obs_scene_item_canvas_scale(struct vec2 *dst, const struct vec2 *scale, float canvas_y, float scale_ref_y)
{
	float scale_factor = canvas_y / scale_ref_y;
	vec2_mulf(dst, scale, scale_factor);
}

void obs_scene_item_relative_scale(struct vec2 *dst, const struct vec2 *v, float canvas_y, float scale_ref_y)
{
	float scale_factor = scale_ref_y / canvas_y;
	vec2_mulf(dst, v, scale_factor);
}
