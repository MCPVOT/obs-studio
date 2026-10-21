/* Test-only oracle: libobs/obs-scene-coords.c.h, unmodified, with the
 * nine functions renamed so they can link next to the Rust shims.
 * vec2_mulf and floorf stay the header and libm originals. */
#define obs_scene_coords_nudge oracle_obs_scene_coords_nudge
#define obs_scene_coords_keep_scale oracle_obs_scene_coords_keep_scale
#define obs_scene_nudge_abs oracle_obs_scene_nudge_abs
#define obs_scene_pos_to_absolute oracle_obs_scene_pos_to_absolute
#define obs_scene_pos_from_absolute oracle_obs_scene_pos_from_absolute
#define obs_scene_size_to_absolute oracle_obs_scene_size_to_absolute
#define obs_scene_size_from_absolute oracle_obs_scene_size_from_absolute
#define obs_scene_item_canvas_scale oracle_obs_scene_item_canvas_scale
#define obs_scene_item_relative_scale oracle_obs_scene_item_relative_scale

#include "obs-scene-coords.c.h"

#undef obs_scene_item_relative_scale
#undef obs_scene_item_canvas_scale
#undef obs_scene_size_from_absolute
#undef obs_scene_size_to_absolute
#undef obs_scene_pos_from_absolute
#undef obs_scene_pos_to_absolute
#undef obs_scene_nudge_abs
#undef obs_scene_coords_keep_scale
#undef obs_scene_coords_nudge
