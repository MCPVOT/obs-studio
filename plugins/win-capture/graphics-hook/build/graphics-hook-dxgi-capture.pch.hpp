/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.dxgi-capture-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* dxgi-capture.cpp */
#include <d3d10_1.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <d3dcompiler.h>
#include <inttypes.h>

#include "plugins/win-capture/graphics-hook/graphics-hook.h"

#include <detours.h>

#if COMPILE_D3D12_HOOK
#include <d3d12.h>
#endif
