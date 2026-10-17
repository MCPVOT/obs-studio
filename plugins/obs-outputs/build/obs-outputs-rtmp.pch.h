/* Precompiled header (BUILD.md) for the plugins/obs-outputs unity groups
 * obs-outputs.rtmp. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* rtmp-stream.c */
#include "plugins/obs-outputs/rtmp-stream.h"
#include "plugins/obs-outputs/rtmp-av1.h"
#include "plugins/obs-outputs/rtmp-hevc.h"

#include <obs-avc.h>
#include <obs-hevc.h>

#include <jansson.h>

#ifdef _WIN32
#include <util/windows/win-version.h>
#endif
