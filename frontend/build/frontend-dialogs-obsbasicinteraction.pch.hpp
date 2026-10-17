/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.dialogs-obsbasicinteraction. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* dialogs/OBSBasicInteraction.cpp */
#include "frontend/dialogs/OBSBasicInteraction.hpp"

#include <dialogs/OBSBasicProperties.hpp>
#include <utility/OBSEventFilter.hpp>
#include <utility/display-helpers.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#include <QInputEvent>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <Windows.h>
#endif
