/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

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

#include "libobs/build/libobs.pch.h"

gs_technique_t *gs_effect_get_technique(const gs_effect_t *effect, const char *name)
{
	if (!effect)
		return NULL;

	for (size_t i = 0; i < effect->techniques.num; i++) {
		struct gs_effect_technique *tech = effect->techniques.array + i;
		if (strcmp(tech->name, name) == 0)
			return tech;
	}

	return NULL;
}

gs_technique_t *gs_effect_get_current_technique(const gs_effect_t *effect)
{
	if (!effect)
		return NULL;

	return effect->cur_technique;
}

size_t gs_effect_get_num_params(const gs_effect_t *effect)
{
	return effect ? effect->params.num : 0;
}

gs_eparam_t *gs_effect_get_param_by_idx(const gs_effect_t *effect, size_t param)
{
	if (!effect)
		return NULL;

	struct gs_effect_param *params = effect->params.array;
	if (param >= effect->params.num)
		return NULL;

	return params + param;
}

gs_eparam_t *gs_effect_get_param_by_name(const gs_effect_t *effect, const char *name)
{
	if (!effect)
		return NULL;

	struct gs_effect_param *params = effect->params.array;

	for (size_t i = 0; i < effect->params.num; i++) {
		struct gs_effect_param *param = params + i;

		if (strcmp(param->name, name) == 0)
			return param;
	}

	return NULL;
}

size_t gs_param_get_num_annotations(const gs_eparam_t *param)
{
	return param ? param->annotations.num : 0;
}

gs_eparam_t *gs_param_get_annotation_by_idx(const gs_eparam_t *param, size_t annotation)
{
	if (!param)
		return NULL;

	struct gs_effect_param *params = param->annotations.array;
	if (annotation > param->annotations.num)
		return NULL;

	return params + annotation;
}

gs_eparam_t *gs_param_get_annotation_by_name(const gs_eparam_t *param, const char *name)
{
	if (!param)
		return NULL;
	struct gs_effect_param *params = param->annotations.array;

	for (size_t i = 0; i < param->annotations.num; i++) {
		struct gs_effect_param *g_param = params + i;
		if (strcmp(g_param->name, name) == 0)
			return g_param;
	}
	return NULL;
}

gs_epass_t *gs_technique_get_pass_by_idx(const gs_technique_t *technique, size_t pass)
{
	if (!technique)
		return NULL;
	struct gs_effect_pass *passes = technique->passes.array;

	if (pass > technique->passes.num)
		return NULL;

	return passes + pass;
}

gs_epass_t *gs_technique_get_pass_by_name(const gs_technique_t *technique, const char *name)
{
	if (!technique)
		return NULL;
	struct gs_effect_pass *passes = technique->passes.array;

	for (size_t i = 0; i < technique->passes.num; i++) {
		struct gs_effect_pass *g_pass = passes + i;
		if (strcmp(g_pass->name, name) == 0)
			return g_pass;
	}
	return NULL;
}

gs_eparam_t *gs_effect_get_viewproj_matrix(const gs_effect_t *effect)
{
	return effect ? effect->view_proj : NULL;
}

gs_eparam_t *gs_effect_get_world_matrix(const gs_effect_t *effect)
{
	return effect ? effect->world : NULL;
}

void gs_effect_get_param_info(const gs_eparam_t *param, struct gs_effect_param_info *info)
{
	if (!param)
		return;

	info->name = param->name;
	info->type = param->type;
}
