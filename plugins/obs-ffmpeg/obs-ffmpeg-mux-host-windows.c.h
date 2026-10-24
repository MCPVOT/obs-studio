#include "plugins/obs-ffmpeg/build/obs-ffmpeg-mux-host-windows.pch.h"

const char *ffmpeg_mux_host_exe_name(void)
{
	return "obs-ffmpeg-mux.exe";
}

/* Windows 10 and later, whose Defender can block the write. */
bool ffmpeg_mux_host_warn_defender(void)
{
	struct win_version_info ver;
	get_win_ver(&ver);
	return ver.major >= 10;
}
