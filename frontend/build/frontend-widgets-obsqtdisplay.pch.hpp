/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.widgets-obsqtdisplay. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* widgets/OBSQTDisplay.cpp */
#include "frontend/widgets/OBSQTDisplay.hpp"

#include <utility/display-helpers.hpp>
#include <utility/SurfaceEventFilter.hpp>

#if !defined(_WIN32) && !defined(__APPLE__)
#include <obs-nix-platform.h>
#endif

#include <QWindow>
#ifdef ENABLE_WAYLAND
#include <QApplication>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif
