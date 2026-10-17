/* Precompiled header (BUILD.md) for the plugins/frontend-tools unity groups
 * frontend-tools.linux. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* auto-scene-switcher-nix.cpp */
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#undef Bool
#undef CursorShape
#undef Expose
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Status
#undef Unsorted
#include <util/platform.h>
#include "plugins/frontend-tools/auto-scene-switcher.hpp"
