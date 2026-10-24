#include "plugins/obs-ffmpeg/build/obs-ffmpeg-host-posix.pch.h"

/* obs-ffmpeg's macOS and BSD host (#853): no device enumeration, so NVENC
 * depends only on loading libnvidia-encode. */

bool obs_ffmpeg_host_nvenc_available(bool (*is_blacklisted)(int device_id))
{
	(void)is_blacklisted;

	void *const lib = os_dlopen("libnvidia-encode.so.1");
	if (!lib)
		return false;
	os_dlclose(lib);
	return true;
}

void obs_ffmpeg_host_load(void) {}

void obs_ffmpeg_host_unload(void) {}
