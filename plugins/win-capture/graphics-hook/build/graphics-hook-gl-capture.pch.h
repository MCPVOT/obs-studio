/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.gl-capture-windows. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* gl-capture.c */
#ifdef _MSC_VER
#pragma warning(disable : 4214) /* nonstandard extension, non-int bitfield */
#pragma warning(disable : 4054) /* function pointer to data pointer */
#endif

#include <windows.h>

#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>

#include "plugins/win-capture/graphics-hook/gl-decs.h"
#include "plugins/win-capture/graphics-hook/graphics-hook.h"

#include <detours.h>
