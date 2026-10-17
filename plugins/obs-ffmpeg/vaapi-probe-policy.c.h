// SPDX-License-Identifier: GPL-2.0-or-later

#include "build/obs-ffmpeg-vaapi-probe-policy.pch.h"

/* RED: stubs, replaced by the implementation in the next commit. */

bool vaapi_driver_is_decode_only(const char *driver_link, const char *libva_driver_name)
{
	UNUSED_PARAMETER(driver_link);
	UNUSED_PARAMETER(libva_driver_name);
	return false;
}

uint32_t vaapi_caps_cache_get(struct vaapi_caps_cache *cache, const char *device_path, vaapi_probe_fn probe,
			      void *param)
{
	UNUSED_PARAMETER(cache);
	return probe(device_path, param);
}

bool vaapi_caps_cache_has(const struct vaapi_caps_cache *cache, const char *device_path)
{
	UNUSED_PARAMETER(cache);
	UNUSED_PARAMETER(device_path);
	return false;
}

void vaapi_caps_cache_add(struct vaapi_caps_cache *cache, const char *device_path, uint32_t flags)
{
	UNUSED_PARAMETER(cache);
	UNUSED_PARAMETER(device_path);
	UNUSED_PARAMETER(flags);
}

void vaapi_caps_cache_free(struct vaapi_caps_cache *cache)
{
	UNUSED_PARAMETER(cache);
}
