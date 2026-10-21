/* Precompiled header (BUILD.md) for the plugins/obs-vst unity groups
 * obs-vst.macos. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* mac/EditorWidget-osx.mm */
#import "plugins/obs-vst/headers/EditorWidget.h"
#import <Cocoa/Cocoa.h>
#include <QLayout>
#include <QWindow>

#import "plugins/obs-vst/headers/VSTPlugin.h"

#include "plugins/obs-vst/headers/vst-platform.hpp"
#include <QFile>
