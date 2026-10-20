/* Test-only oracle: libobs/obs-scene-bounds.c.h, unmodified, with
 * obs_bounds_fit_scale renamed so it can link next to the Rust shim.
 * vec2_mulf stays the header inline. */
#define obs_bounds_fit_scale oracle_obs_bounds_fit_scale

#include "obs-scene-bounds.c.h"
