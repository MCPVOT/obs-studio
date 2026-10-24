/* Precompiled header (BUILD.md) for the plugins/obs-ffmpeg/ffmpeg-mux unity groups
 * ffmpeg-mux.core. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* ffmpeg-mux.c */
#include <stdio.h>
#include <stdlib.h>
#include "plugins/obs-ffmpeg/ffmpeg-mux/ffmpeg-mux.h"
#include "plugins/obs-ffmpeg/ffmpeg-mux/ffmpeg-mux-host.h"

#include <util/threading.h>
#include <util/platform.h>
#include <util/deque.h>
#include <util/dstr.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mastering_display_metadata.h>
