/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.d3d9-capture-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* d3d9-capture.cpp */
#include <d3d9.h>
#include <d3d11.h>
#include <dxgi.h>

#include "plugins/win-capture/graphics-hook/graphics-hook.h"
#include "plugins/win-capture/graphics-hook/d3d9-patches.hpp"

#include <detours.h>
