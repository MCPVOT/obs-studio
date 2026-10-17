/* Precompiled header (BUILD.md) for the plugins/obs-vst unity groups
 * obs-vst.windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* win/EditorWidget-win.cpp */
#include <QGridLayout>
#include "plugins/obs-vst/headers/EditorWidget.h"

/* win/VSTPlugin-win.cpp */
#include "plugins/obs-vst/headers/VSTPlugin.h"
#include "plugins/obs-vst/headers/vst-plugin-callbacks.hpp"

#include <util/platform.h>
#include <windows.h>
