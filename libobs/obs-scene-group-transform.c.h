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

#include "libobs/build/libobs-scene-group-transform.pch.h"

void obs_scene_transform_val(struct vec2 *dst, const struct matrix4 *transform)
{
	struct vec3 v;
	struct matrix4 copied;

	vec3_set(&v, dst->x, dst->y, 0.0f);
	matrix4_copy(&copied, transform);
	vec3_transform(&v, &v, &copied);
	dst->x = v.x;
	dst->y = v.y;
}

void obs_scene_group_transform(const struct matrix4 *transform, struct vec2 *pos, struct vec2 *scale, float *rot,
			       float rot_delta)
{
	struct matrix4 copied;
	struct matrix4 mat;
	struct vec4 x_base;

	/* Written and never read in the original. Kept so the body matches. */
	vec4_set(&x_base, 1.0f, 0.0f, 0.0f, 0.0f);

	matrix4_copy(&copied, transform);

	obs_scene_transform_val(pos, &copied);
	vec4_set(&copied.t, 0.0f, 0.0f, 0.0f, 1.0f);

	vec4_set(&mat.x, scale->x, 0.0f, 0.0f, 0.0f);
	vec4_set(&mat.y, 0.0f, scale->y, 0.0f, 0.0f);
	vec4_set(&mat.z, 0.0f, 0.0f, 1.0f, 0.0f);
	vec4_set(&mat.t, 0.0f, 0.0f, 0.0f, 1.0f);
	matrix4_mul(&mat, &mat, &copied);

	scale->x = vec4_len(&mat.x) * (scale->x > 0.0f ? 1.0f : -1.0f);
	scale->y = vec4_len(&mat.y) * (scale->y > 0.0f ? 1.0f : -1.0f);

	*rot += rot_delta;
}
