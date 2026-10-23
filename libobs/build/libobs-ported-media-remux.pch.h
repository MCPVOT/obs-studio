/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.ported-media-remux. media-remux.c needs only its own header,
 * libavformat and the libobs allocator, log and file helpers, so the C oracle
 * can compile it against its FFmpeg stand-in. The implementation file
 * includes this header and nothing else. */

#pragma once

#include "libobs/media-io/media-remux.h"
#include "libobs/util/base.h"
#include "libobs/util/bmem.h"
#include "libobs/util/platform.h"

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/channel_layout.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
