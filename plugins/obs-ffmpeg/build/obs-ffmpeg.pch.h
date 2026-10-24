/* Precompiled header (BUILD.md) for the plugins/obs-ffmpeg unity groups
 * obs-ffmpeg.core, obs-ffmpeg.logging. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-ffmpeg-audio-encoders.c */
#include <util/base.h>
#include <util/deque.h>
#include <util/darray.h>
#include <util/dstr.h>
#include <obs-module.h>

#include <libavutil/channel_layout.h>
#include <libavformat/avformat.h>

#include "plugins/obs-ffmpeg/obs-ffmpeg-formats.h"
#include "plugins/obs-ffmpeg/obs-ffmpeg-compat.h"

/* obs-ffmpeg.c */
#include <util/platform.h>
#include <libavutil/avutil.h>
#include <libavcodec/avcodec.h>

#include "plugins/obs-ffmpeg/obs-ffmpeg-host.h"

#ifdef LIBAVUTIL_VAAPI_AVAILABLE
#include "plugins/obs-ffmpeg/vaapi-utils.h"
#endif

/* obs-ffmpeg-logging.c */
#include <util/threading.h>
#include <libavutil/log.h>
