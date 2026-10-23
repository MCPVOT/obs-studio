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

#include "libobs/build/libobs-ported-service-getters.pch.h"

/* Service capability and connect-info getters, moved from obs-service.c
 * (#1899): each checks its arguments, writes its out-parameters' defaults
 * and calls the plugin's optional callback with the service's data. They
 * reach the service only through the hidden obs_service_get_info_table()
 * and obs_service_get_plugin_data() (obs-service.c), so the Rust port (obs-output's
 * service module) can replace this file. test_service_getters pins what
 * they do, including two cases that are characterized, not endorsed:
 * obs_service_get_max_bitrate() zeroes its out-parameters before it checks
 * the service, and the codec getters do not check for a NULL service. */

#define service_info(service) obs_service_get_info_table(service)
#define service_data(service) obs_service_get_plugin_data(service)

void obs_service_get_supported_resolutions(const obs_service_t *service, struct obs_service_resolution **resolutions,
					   size_t *count)
{
	if (!obs_service_valid(service, "obs_service_supported_resolutions"))
		return;
	if (!obs_ptr_valid(resolutions, "obs_service_supported_resolutions"))
		return;
	if (!obs_ptr_valid(count, "obs_service_supported_resolutions"))
		return;

	*resolutions = NULL;
	*count = 0;

	if (service_info(service)->get_supported_resolutions)
		service_info(service)->get_supported_resolutions(service_data(service), resolutions, count);
}

void obs_service_get_max_fps(const obs_service_t *service, int *fps)
{
	if (!obs_service_valid(service, "obs_service_get_max_fps"))
		return;
	if (!obs_ptr_valid(fps, "obs_service_get_max_fps"))
		return;

	*fps = 0;

	if (service_info(service)->get_max_fps)
		service_info(service)->get_max_fps(service_data(service), fps);
}

void obs_service_get_max_bitrate(const obs_service_t *service, int *video_bitrate, int *audio_bitrate)
{
	if (video_bitrate)
		*video_bitrate = 0;
	if (audio_bitrate)
		*audio_bitrate = 0;

	if (!obs_service_valid(service, "obs_service_get_max_bitrate"))
		return;

	if (service_info(service)->get_max_bitrate)
		service_info(service)->get_max_bitrate(service_data(service), video_bitrate, audio_bitrate);
}

const char **obs_service_get_supported_video_codecs(const obs_service_t *service)
{
	if (service_info(service)->get_supported_video_codecs)
		return service_info(service)->get_supported_video_codecs(service_data(service));
	return NULL;
}

const char **obs_service_get_supported_audio_codecs(const obs_service_t *service)
{
	if (service_info(service)->get_supported_audio_codecs)
		return service_info(service)->get_supported_audio_codecs(service_data(service));
	return NULL;
}

const char *obs_service_get_protocol(const obs_service_t *service)
{
	if (!obs_service_valid(service, "obs_service_get_protocol"))
		return NULL;

	return service_info(service)->get_protocol(service_data(service));
}

const char *obs_service_get_preferred_output_type(const obs_service_t *service)
{
	if (!obs_service_valid(service, "obs_service_get_preferred_output_type"))
		return NULL;

	if (service_info(service)->get_output_type)
		return service_info(service)->get_output_type(service_data(service));
	return NULL;
}

const char *obs_service_get_connect_info(const obs_service_t *service, uint32_t type)
{
	if (!obs_service_valid(service, "obs_service_get_info"))
		return NULL;

	if (!service_info(service)->get_connect_info)
		return NULL;
	return service_info(service)->get_connect_info(service_data(service), type);
}

bool obs_service_can_try_to_connect(const obs_service_t *service)
{
	if (!obs_service_valid(service, "obs_service_can_connect"))
		return false;

	if (!service_info(service)->can_try_to_connect)
		return true;
	return service_info(service)->can_try_to_connect(service_data(service));
}

/* File-local macros: keep them out of the next source of a unity build. */
#undef service_info
#undef service_data
