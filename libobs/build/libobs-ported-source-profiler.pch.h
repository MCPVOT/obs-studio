/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.ported-source-profiler. util/source-profiler.c reads core state
 * (obs->video.graphics, obs_source.info), so this is obs-internal.h plus the
 * source-profiler slice of libobs.pch.h. The implementation file includes
 * this header and nothing else. */

#pragma once

#include <math.h>

#include "obs-internal.h"
#include "util/source-profiler.h"
#include "util/util_uint64.h"

#include "util/uthash.h"
