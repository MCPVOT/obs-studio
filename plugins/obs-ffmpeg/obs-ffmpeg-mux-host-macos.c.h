#include "plugins/obs-ffmpeg/build/obs-ffmpeg-mux-host-macos.pch.h"

const char *ffmpeg_mux_host_exe_name(void)
{
	return "obs-ffmpeg-mux";
}

bool ffmpeg_mux_host_warn_defender(void)
{
	return false;
}
