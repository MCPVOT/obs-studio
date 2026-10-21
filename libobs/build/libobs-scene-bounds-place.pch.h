/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.scene-bounds-place. obs-scene-bounds-place.c.h alone, without the
 * core, so a C test can compile it. The implementation file includes this
 * header and nothing else.
 *
 * enum obs_bounds_type and struct obs_sceneitem_crop live in obs.h. Same
 * order and layout, so the values match. obs.h is not included here: this
 * file is compiled without the core. Keep these in lockstep with obs.h and
 * with libobs/build/libobs-ported-scene-bounds.pch.h. */

#pragma once

#include "graphics/vec2.h"
#include "obs-defs.h"

enum obs_bounds_type {
	OBS_BOUNDS_NONE,
	OBS_BOUNDS_STRETCH,
	OBS_BOUNDS_SCALE_INNER,
	OBS_BOUNDS_SCALE_OUTER,
	OBS_BOUNDS_SCALE_TO_WIDTH,
	OBS_BOUNDS_SCALE_TO_HEIGHT,
	OBS_BOUNDS_MAX_ONLY,
};

struct obs_sceneitem_crop {
	int left;
	int top;
	int right;
	int bottom;
};

void obs_bounds_fit_scale(enum obs_bounds_type type, const struct vec2 *bounds, uint32_t cx, uint32_t cy,
			  struct vec2 *scale);

void add_alignment(struct vec2 *v, uint32_t align, int cx, int cy);

void obs_bounds_place(enum obs_bounds_type type, const struct vec2 *bounds, struct vec2 *origin, struct vec2 *scale,
		      uint32_t *cx, uint32_t *cy, uint32_t align, bool crop_to_bounds, struct obs_sceneitem_crop *crop);
