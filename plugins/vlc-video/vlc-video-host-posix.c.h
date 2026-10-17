#include "plugins/vlc-video/build/vlc-video-posix.pch.h"

void *vlc_host_open_libvlc(void)
{
	return os_dlopen("libvlc.so.5");
}

void vlc_host_unload(void) {}

void vlc_host_native_path(struct dstr *path, bool is_url)
{
	UNUSED_PARAMETER(path);
	UNUSED_PARAMETER(is_url);
}
