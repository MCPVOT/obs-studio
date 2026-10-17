/* Precompiled header (BUILD.md) for the plugins/obs-qsv11 unity groups
 * obs-qsv11.windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* common_directx11.cpp */
#include "plugins/obs-qsv11/common_directx11.h"

#include <obs.h>
#include <obs-encoder.h>
#include <map>

/* common_utils_windows.cpp */
#include "plugins/obs-qsv11/common_utils.h"

#include <util/windows/device-enum.h>
#include <util/config-file.h>
#include <util/platform.h>
#include <util/pipe.h>
#include <util/dstr.h>

#include <intrin.h>
#include <inttypes.h>
#include <obs-module.h>

/* qsv-host interface (#860) */
#include "plugins/obs-qsv11/qsv-host.h"
