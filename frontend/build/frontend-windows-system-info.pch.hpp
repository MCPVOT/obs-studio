/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.windows-system-info. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/system-info-windows.cpp */
#include "frontend/utility/system-info.hpp"

#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/windows/ComPtr.hpp>
#include <util/windows/win-registry.h>
#include <util/windows/win-version.h>

#include <cinttypes>

#include <dxgi.h>
#include <shlobj.h>
