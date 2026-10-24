/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.windows-obs-main. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/obs-main-host-windows.cpp */
#include <OBSApp.hpp>
#include <utility/platform.hpp>

#include <obs.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/windows/win-version.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

#include <shellapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <utility/obs-main-windows.hpp>
