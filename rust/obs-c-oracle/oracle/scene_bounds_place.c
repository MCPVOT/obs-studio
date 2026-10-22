/* Original placement source, with every dependency kept on the C side. */
#define obs_bounds_place oracle_obs_bounds_place
#define add_alignment oracle_add_alignment
#define obs_bounds_fit_scale oracle_obs_bounds_fit_scale
#include "obs-scene-bounds-place.c.h"
#undef obs_bounds_fit_scale
#undef add_alignment
#undef obs_bounds_place
