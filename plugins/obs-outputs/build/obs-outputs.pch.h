/* Precompiled header (BUILD.md) for the plugins/obs-outputs unity groups
 * obs-outputs.core. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* flv-mux.c */
#include <obs.h>
#include <stdio.h>
#include <util/dstr.h>
#include <util/array-serializer.h>
#include "plugins/obs-outputs/flv-mux.h"
#include "plugins/obs-outputs/obs-output-ver.h"
#include "plugins/obs-outputs/rtmp-helpers.h"

/* flv-output.c */
#include <obs-module.h>
#include <obs-avc.h>
#ifdef ENABLE_HEVC
#include "plugins/obs-outputs/rtmp-hevc.h"
#include <obs-hevc.h>
#endif
#include "plugins/obs-outputs/rtmp-av1.h"
#include <util/platform.h>
#include <util/threading.h>
#include <inttypes.h>
#include "plugins/obs-outputs/color-enums.h"

/* mp4-output.c */
#include "plugins/obs-outputs/mp4-mux.h"

#include <util/deque.h>
#include <util/buffered-file-serializer.h>
#include <bpm.h>

#include <opts-parser.h>

/* obs-outputs.c */

#ifdef _WIN32
#include <winsock2.h>
#include <mbedtls/threading.h>
#endif
