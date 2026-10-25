#include "plugins/obs-ffmpeg/build/obs-ffmpeg-mux-host-posix.pch.h"

/* The Rust brand installs it as obs-studio-rust-ffmpeg-mux (#2296). */
#ifndef OBS_FFMPEG_MUX_NAME
#define OBS_FFMPEG_MUX_NAME "obs-ffmpeg-mux"
#endif

const char *ffmpeg_mux_host_exe_name(void)
{
	return OBS_FFMPEG_MUX_NAME;
}

bool ffmpeg_mux_host_warn_defender(void)
{
	return false;
}

#undef OBS_FFMPEG_MUX_NAME
