/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.windows-autoupdatethread. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/AutoUpdateThread.cpp */
#include "frontend/utility/AutoUpdateThread.hpp"
#include "ui_OBSUpdate.h"

#include <OBSApp.hpp>
#include <dialogs/OBSUpdate.hpp>
#include <updater/manifest.hpp>
#include <utility/WhatsNewInfoThread.hpp>
#include <utility/update-helpers.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>

/* The classes' own headers, which their moc output used to include. */
#include "frontend/utility/AutoUpdateThread.hpp"
