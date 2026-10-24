#pragma once

/* The FFmpeg muxer output's host interface (obs-rust/obs-studio#853): what
 * differs between Windows, macOS and the other POSIX hosts, so
 * obs-ffmpeg-mux.c needs no platform conditionals. Each host lives in
 * obs-ffmpeg-mux-host-<os>.c; CMake picks one (BUILD.md). */

#include <stdbool.h>

/* The muxer helper's file name, next to the OBS executable. */
const char *ffmpeg_mux_host_exe_name(void);

/* Whether "unable to write" errors add the Windows Defender hint (Windows 10
 * and later, where Controlled Folder Access can block the write). */
bool ffmpeg_mux_host_warn_defender(void);
