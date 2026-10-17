/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.plugin-manager-mm. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* In the unity group the Objective-C++ files are included headers, where
 * clang checks nullability completeness (as mac-virtualcam's, #1212). */
#pragma clang diagnostic ignored "-Wnullability-completeness"

/* plugin-manager/PluginModuleLoader_MacOS.mm */
#include "frontend/plugin-manager/PluginModuleLoader.hpp"
#include "frontend/plugin-manager/PluginManager.hpp"

#include <obs.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <string>
#include <string_view>
#include <vector>
