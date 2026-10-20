/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.ported-scene-bounds. obs-scene-bounds.c alone, without the core,
 * so the C oracle and C tests can compile it. The implementation file
 * includes this header and nothing else. */

#pragma once

#include "graphics/vec2.h"

/* enum obs_bounds_type lives in obs.h. Same order, so the values match.
 * obs.h is not included here: this file is compiled without the core. */
enum obs_bounds_type {
	OBS_BOUNDS_NONE,
	OBS_BOUNDS_STRETCH,
	OBS_BOUNDS_SCALE_INNER,
	OBS_BOUNDS_SCALE_OUTER,
	OBS_BOUNDS_SCALE_TO_WIDTH,
	OBS_BOUNDS_SCALE_TO_HEIGHT,
	OBS_BOUNDS_MAX_ONLY,
};

void obs_bounds_fit_scale(enum obs_bounds_type type, const struct vec2 *bounds, uint32_t cx, uint32_t cy,
			  struct vec2 *scale);
