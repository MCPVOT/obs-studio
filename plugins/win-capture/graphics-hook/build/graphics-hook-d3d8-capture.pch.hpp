/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.d3d8-capture-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* d3d8-capture.cpp */
#include <dxgi.h>

#ifdef OBS_LEGACY
#include "../d3d8-api/d3d8.h"
#else
#include <d3d8.h>
#endif
#include "plugins/win-capture/graphics-hook/graphics-hook.h"

#include <detours.h>
