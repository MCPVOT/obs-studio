/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.d3d12-capture-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* d3d12-capture.cpp */
#include <windows.h>
#include "plugins/win-capture/graphics-hook/graphics-hook.h"

#if COMPILE_D3D12_HOOK
#include <d3d11on12.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <inttypes.h>

#include <detours.h>

#include "plugins/win-capture/graphics-hook/dxgi-helpers.hpp"
#endif
