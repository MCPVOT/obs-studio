/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.graphics-hook-windows. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* graphics-hook.c */
#include <windows.h>
#include <psapi.h>
#include <inttypes.h>
#include "plugins/win-capture/graphics-hook/graphics-hook.h"
#ifdef OBS_LEGACY
#include "../graphics-hook-ver.h"
#include "../../libobs/util/windows/obfuscate.h"
#else
#include <graphics-hook-ver.h>
#include <util/windows/obfuscate.h>
#endif
