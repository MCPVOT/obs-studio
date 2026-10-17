/* Precompiled header (BUILD.md) for the frontend unity group
 * frontend.plugin-manager-linux. C++. PluginModuleLoader_Linux.cpp includes this
 * header and nothing else; each loader has its own, as each build generates
 * only its own LoaderPaths header. */

#pragma once

#include "frontend/plugin-manager/PluginModuleLoader.hpp"
#include "frontend/plugin-manager/PluginManager.hpp"
#include <LoaderPaths_Linux.hpp>

#include <obs.h>
#include <util/dstr.h>
#include <util/platform.h>

#include <array>
#include <string>
#include <string_view>
#include <vector>
