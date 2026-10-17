/* Precompiled header (BUILD.md) for the plugins/obs-vst unity groups
 * obs-vst.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* EditorWidget.cpp */
#include "plugins/obs-vst/headers/EditorWidget.h"
#include <QCloseEvent>

/* VSTPlugin.cpp */
#include "plugins/obs-vst/headers/VSTPlugin.h"
#include <util/platform.h>

/* obs-vst.cpp */
#include <QCryptographicHash>
