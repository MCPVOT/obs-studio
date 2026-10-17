/* Precompiled header (BUILD.md) for the shared/obs-scripting unity groups
 * obs-scripting.lua. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-scripting-lua-source.c */
#include "shared/obs-scripting/obs-scripting-lua.h"
#include "shared/obs-scripting/cstrcache.h"

#include <obs-module.h>
#include <util/platform.h>

/* obs-scripting-lua.c */
#include <util/base.h>
#include <util/dstr.h>

#include <obs.h>

/* ========================================================================= */

#if ARCH_BITS == 64
#define ARCH_DIR "64bit"
#else
#define ARCH_DIR "32bit"
#endif

#ifdef __APPLE__
#define SO_EXT "so"
#elif defined(_WIN32)
#include <windows.h>
#define SO_EXT "dll"
#else
#define SO_EXT "so"
#endif
