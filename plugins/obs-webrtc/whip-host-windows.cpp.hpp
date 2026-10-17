#include "plugins/obs-webrtc/build/obs-webrtc-windows.pch.hpp"

const char *whip_host_os_name(void)
{
	/* 32-bit Windows has always reported "Linux" here: the old #ifdef chain
	 * only named _WIN64 and fell through to its last branch. Kept as is. */
	return sizeof(void *) == 8 ? "Windows x86_64" : "Linux";
}
