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

#include "libobs/build/libobs-ported-graphics.pch.h"

void shader_sampler_convert(struct shader_sampler *ss, struct gs_sampler_info *info)
{
	size_t i;
	memset(info, 0, sizeof(struct gs_sampler_info));

	info->max_anisotropy = 1;

	for (i = 0; i < ss->states.num; i++) {
		const char *state = ss->states.array[i];
		const char *value = ss->values.array[i];

		if (astrcmpi(state, "Filter") == 0)
			info->filter = get_sample_filter(value);
		else if (astrcmpi(state, "AddressU") == 0)
			info->address_u = get_address_mode(value);
		else if (astrcmpi(state, "AddressV") == 0)
			info->address_v = get_address_mode(value);
		else if (astrcmpi(state, "AddressW") == 0)
			info->address_w = get_address_mode(value);
		else if (astrcmpi(state, "MaxAnisotropy") == 0)
			info->max_anisotropy = (int)strtol(value, NULL, 10);
		else if (astrcmpi(state, "BorderColor") == 0)
			info->border_color = (uint32_t)strtol(value + 1, NULL, 16);
	}
}
