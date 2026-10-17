/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.plugin-manager-pluginmoduleloader_windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* plugin-manager/PluginModuleLoader_Windows.cpp */
#include "frontend/plugin-manager/PluginModuleLoader.hpp"
#include "frontend/plugin-manager/PluginManager.hpp"

#include <obs.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <string>
#include <string_view>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
