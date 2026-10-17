/* Precompiled header (BUILD.md) for the plugins/obs-ffmpeg unity groups
 * obs-ffmpeg.mux. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-ffmpeg-hls-mux.c */
#include "plugins/obs-ffmpeg/obs-ffmpeg-mux.h"
#include <obs-avc.h>
#ifdef ENABLE_HEVC
#include <obs-hevc.h>
#endif

/* obs-ffmpeg-mux.c */
#include "plugins/obs-ffmpeg/ffmpeg-mux/ffmpeg-mux.h"
#include "plugins/obs-ffmpeg/obs-ffmpeg-formats.h"

#ifdef _WIN32
#include "util/windows/win-version.h"
#endif

#include <libavformat/avformat.h>
