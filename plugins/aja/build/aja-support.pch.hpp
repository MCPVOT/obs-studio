/* Precompiled header (BUILD.md) for the plugins/aja unity groups
 * aja.support. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* aja-card-manager.cpp */
#include "plugins/aja/aja-card-manager.hpp"
#include "plugins/aja/aja-common.hpp"
#include "plugins/aja/aja-output.hpp"
#include "plugins/aja/aja-source.hpp"

#include "obs-properties.h"

#include <util/base.h>

#include <ajantv2/includes/ntv2card.h>
#include <ajantv2/includes/ntv2devicescanner.h>
#include <ajantv2/includes/ntv2devicefeatures.h>
#include <ajantv2/includes/ntv2utils.h>

#include <ajabase/system/process.h>
#include <ajabase/system/systemtime.h>

/* aja-common.cpp */
#include "plugins/aja/aja-ui-props.hpp"
#include "plugins/aja/aja-props.hpp"

#include <ajantv2/includes/ntv2debug.h>
#include <ajantv2/includes/ntv2signalrouter.h>

/* aja-presets.cpp */
#include "plugins/aja/aja-presets.hpp"

/* aja-routing.cpp */
#include "plugins/aja/aja-routing.hpp"
#include "plugins/aja/aja-widget-io.hpp"

#include <ajabase/common/common.h>

#include <obs-module.h>

/* aja-vpid-data.cpp */
#include "plugins/aja/aja-vpid-data.hpp"

/* aja-widget-io.cpp */

#include <ajantv2/includes/ntv2enums.h>

#include <iostream>
