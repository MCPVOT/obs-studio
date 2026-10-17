/* Precompiled header (BUILD.md) for the plugins/win-capture/get-graphics-offsets unity groups
 * get-graphics-offsets.d3d8-offsets-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* d3d8-offsets.cpp */
#include <stdio.h>
#include <windows.h>
#ifdef OBS_LEGACY
#include "../d3d8-api/d3d8.h"
#else
#include <d3d8.h>
#endif
#include "plugins/win-capture/get-graphics-offsets/get-graphics-offsets.h"
