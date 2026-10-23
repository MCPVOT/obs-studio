/* Test-only oracle: the original libobs/graphics/effect-lookups.c.h,
 * unmodified, with its exports renamed to oracle_* so it can link next to
 * the Rust implementation. */
#define gs_effect_get_technique oracle_gs_effect_get_technique
#define gs_effect_get_current_technique oracle_gs_effect_get_current_technique
#define gs_effect_get_num_params oracle_gs_effect_get_num_params
#define gs_effect_get_param_by_idx oracle_gs_effect_get_param_by_idx
#define gs_effect_get_param_by_name oracle_gs_effect_get_param_by_name
#define gs_param_get_num_annotations oracle_gs_param_get_num_annotations
#define gs_param_get_annotation_by_idx oracle_gs_param_get_annotation_by_idx
#define gs_param_get_annotation_by_name oracle_gs_param_get_annotation_by_name
#define gs_technique_get_pass_by_idx oracle_gs_technique_get_pass_by_idx
#define gs_technique_get_pass_by_name oracle_gs_technique_get_pass_by_name
#define gs_effect_get_viewproj_matrix oracle_gs_effect_get_viewproj_matrix
#define gs_effect_get_world_matrix oracle_gs_effect_get_world_matrix
#define gs_effect_get_param_info oracle_gs_effect_get_param_info

#include "graphics/effect-lookups.c.h"

#include <stddef.h>

#define SIZE_ALIGN(ty, stem)                             \
	size_t oracle_##stem##_size(void);               \
	size_t oracle_##stem##_size(void)                \
	{                                                \
		return sizeof(ty);                      \
	}                                                \
	size_t oracle_##stem##_align(void);              \
	size_t oracle_##stem##_align(void)               \
	{                                                \
		return _Alignof(ty);                    \
	}

#define FIELD(ty, stem, field)                           \
	size_t oracle_##stem##_offset_##field(void);    \
	size_t oracle_##stem##_offset_##field(void)     \
	{                                                \
		return offsetof(ty, field);             \
	}

SIZE_ALIGN(struct gs_effect_param, effect_param)
FIELD(struct gs_effect_param, effect_param, name)
FIELD(struct gs_effect_param, effect_param, section)
FIELD(struct gs_effect_param, effect_param, type)
FIELD(struct gs_effect_param, effect_param, changed)
FIELD(struct gs_effect_param, effect_param, cur_val)
FIELD(struct gs_effect_param, effect_param, default_val)
FIELD(struct gs_effect_param, effect_param, effect)
FIELD(struct gs_effect_param, effect_param, next_sampler)
FIELD(struct gs_effect_param, effect_param, annotations)

SIZE_ALIGN(struct gs_effect_pass, effect_pass)
FIELD(struct gs_effect_pass, effect_pass, name)
FIELD(struct gs_effect_pass, effect_pass, section)
FIELD(struct gs_effect_pass, effect_pass, vertshader)
FIELD(struct gs_effect_pass, effect_pass, pixelshader)
FIELD(struct gs_effect_pass, effect_pass, vertshader_params)
FIELD(struct gs_effect_pass, effect_pass, pixelshader_params)

SIZE_ALIGN(struct gs_effect_technique, effect_technique)
FIELD(struct gs_effect_technique, effect_technique, name)
FIELD(struct gs_effect_technique, effect_technique, section)
FIELD(struct gs_effect_technique, effect_technique, effect)
FIELD(struct gs_effect_technique, effect_technique, passes)

SIZE_ALIGN(struct gs_effect, effect)
FIELD(struct gs_effect, effect, processing)
FIELD(struct gs_effect, effect, cached)
FIELD(struct gs_effect, effect, effect_path)
FIELD(struct gs_effect, effect, effect_dir)
FIELD(struct gs_effect, effect, params)
FIELD(struct gs_effect, effect, techniques)
FIELD(struct gs_effect, effect, cur_technique)
FIELD(struct gs_effect, effect, cur_pass)
FIELD(struct gs_effect, effect, view_proj)
FIELD(struct gs_effect, effect, world)
FIELD(struct gs_effect, effect, scale)
FIELD(struct gs_effect, effect, graphics)
FIELD(struct gs_effect, effect, next)
FIELD(struct gs_effect, effect, loop_pass)
FIELD(struct gs_effect, effect, looping)

SIZE_ALIGN(struct gs_effect_param_info, effect_param_info)
FIELD(struct gs_effect_param_info, effect_param_info, name)
FIELD(struct gs_effect_param_info, effect_param_info, type)
