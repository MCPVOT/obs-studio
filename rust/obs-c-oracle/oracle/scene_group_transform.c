/* Test-only oracle: libobs/obs-scene-group-transform.c.h, unmodified, with
 * the two functions renamed so they can link next to the Rust shims.
 * matrix4_mul and vec3_transform are the graphics oracle. vec4_len,
 * matrix4_copy, and the other helpers stay the header inlines. */
#define obs_scene_transform_val oracle_obs_scene_transform_val
#define obs_scene_group_transform oracle_obs_scene_group_transform
#define matrix4_mul oracle_matrix4_mul
#define vec3_transform oracle_vec3_transform

#include "obs-scene-group-transform.c.h"

#undef vec3_transform
#undef matrix4_mul
#undef obs_scene_group_transform
#undef obs_scene_transform_val
