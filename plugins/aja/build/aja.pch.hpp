/* Precompiled header (BUILD.md) for the plugins/aja unity groups
 * aja.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* aja-output.cpp */
#include "plugins/aja/aja-card-manager.hpp"
#include "plugins/aja/aja-common.hpp"
#include "plugins/aja/aja-ui-props.hpp"
#include "plugins/aja/aja-output.hpp"
#include "plugins/aja/aja-routing.hpp"

#include <obs-module.h>
#include <util/platform.h>

#include <ajabase/common/timer.h>
#include <ajabase/system/systemtime.h>

#include <ajantv2/includes/ntv2card.h>
#include <ajantv2/includes/ntv2devicefeatures.h>

#include <atomic>
#include <stdlib.h>
#include <inttypes.h>

/* aja-source.cpp */
#include "plugins/aja/aja-source.hpp"

#include <util/threading.h>
#include <util/dstr.h>
#include <util/util_uint64.h>

#include <ajantv2/includes/ntv2utils.h>

/* main.cpp */
#include <ajantv2/includes/ntv2devicescanner.h>
