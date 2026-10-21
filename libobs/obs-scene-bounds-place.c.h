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

#include "libobs/build/libobs-scene-bounds-place.pch.h"

void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy)
{
	if (align & OBS_ALIGN_RIGHT)
		v->x += (float)cx;
	else if ((align & OBS_ALIGN_LEFT) == 0)
		v->x += (float)(cx / 2);

	if (align & OBS_ALIGN_BOTTOM)
		v->y += (float)cy;
	else if ((align & OBS_ALIGN_TOP) == 0)
		v->y += (float)(cy / 2);
}

/* The predicate calculate_bounds_data used to apply before writing
 * bounds_crop. The scene still decides absolute versus relative bounds. */
static inline bool bounds_crop_active(enum obs_bounds_type type, bool crop_to_bounds)
{
	return crop_to_bounds && (type == OBS_BOUNDS_SCALE_OUTER || type == OBS_BOUNDS_SCALE_TO_HEIGHT ||
				  type == OBS_BOUNDS_SCALE_TO_WIDTH);
}

void obs_bounds_place(enum obs_bounds_type type, const struct vec2 *bounds, struct vec2 *origin, struct vec2 *scale,
		      uint32_t *cx, uint32_t *cy, uint32_t align, bool crop_to_bounds, struct obs_sceneitem_crop *crop)
{
	obs_bounds_fit_scale(type, bounds, *cx, *cy, scale);

	float width = (float)(*cx) * scale->x;
	float height = (float)(*cy) * scale->y;
	float width_diff, height_diff;

	/* Disregards flip when calculating size diff */
	width_diff = bounds->x - fabsf(width);
	height_diff = bounds->y - fabsf(height);
	*cx = (uint32_t)bounds->x;
	*cy = (uint32_t)bounds->y;

	add_alignment(origin, align, (int)-width_diff, (int)-height_diff);

	/* Set cropping if enabled and large enough size difference exists */
	if (bounds_crop_active(type, crop_to_bounds) && ((double)width_diff < -0.1 || (double)height_diff < -0.1)) {
		bool crop_width = (double)width_diff < -0.1;
		bool crop_flipped = crop_width ? width < 0.0f : height < 0.0f;

		float crop_diff = crop_width ? width_diff : height_diff;
		float crop_scale = crop_width ? scale->x : scale->y;
		float crop_origin = crop_width ? origin->x : origin->y;

		/* Only get alignment for relevant axis */
		uint32_t crop_align_mask = crop_width ? OBS_ALIGN_LEFT | OBS_ALIGN_RIGHT
						      : OBS_ALIGN_TOP | OBS_ALIGN_BOTTOM;
		uint32_t crop_align = align & crop_align_mask;

		/* Cropping values need to scaled to input source */
		float overdraw = fabsf(crop_diff / crop_scale);

		/* tl = top / left, br = bottom / right */
		float overdraw_tl;
		if (crop_align & (OBS_ALIGN_TOP | OBS_ALIGN_LEFT))
			overdraw_tl = 0;
		else if (crop_align & (OBS_ALIGN_BOTTOM | OBS_ALIGN_RIGHT))
			overdraw_tl = overdraw;
		else
			overdraw_tl = overdraw / 2;

		float overdraw_br = overdraw - overdraw_tl;

		int crop_br, crop_tl;
		if (crop_flipped) {
			/* Adjust origin for flips */
			if (crop_align == OBS_ALIGN_CENTER)
				crop_origin *= 2;
			else if (crop_align & (OBS_ALIGN_TOP | OBS_ALIGN_LEFT))
				crop_origin -= crop_diff;

			/* Note that crops are swapped if the axis is flipped */
			crop_br = (int)roundf(overdraw_tl);
			crop_tl = (int)roundf(overdraw_br);
		} else {
			crop_origin = 0;
			crop_br = (int)roundf(overdraw_br);
			crop_tl = (int)roundf(overdraw_tl);
		}

		if (crop_width) {
			crop->right = crop_br;
			crop->left = crop_tl;
			origin->x = crop_origin;
		} else {
			crop->bottom = crop_br;
			crop->top = crop_tl;
			origin->y = crop_origin;
		}
	}

	/* Makes the item stay in-place in the box if flipped */
	origin->x += (width < 0.0f) ? width : 0.0f;
	origin->y += (height < 0.0f) ? height : 0.0f;
}
