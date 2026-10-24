/* obs-ffmpeg's host interface (obs-rust/obs-studio#853): the NVENC device
 * check and the host's own module load and unload work (AMF on Windows). */

#pragma once

#include <stdbool.h>

/* Whether an NVENC-capable NVIDIA device and driver are present. The shared
 * module passes its adapter blacklist as is_blacklisted(PCI device id). */
bool obs_ffmpeg_host_nvenc_available(bool (*is_blacklisted)(int device_id));

void obs_ffmpeg_host_load(void);
void obs_ffmpeg_host_unload(void);
