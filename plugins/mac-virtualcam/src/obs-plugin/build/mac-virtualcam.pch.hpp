/* Precompiled header (BUILD.md) for the plugins/mac-virtualcam/src/obs-plugin unity groups
 * mac-virtualcam.macos. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* plugin-main.mm (the target builds with -fmodules) */
@import CoreMediaIO;
@import SystemExtensions;

/* OBSDALMachServer.mm */
#import "../OBSDALMachServer.h"
#include <obs-module.h>
#include "MachProtocol.h"
#include "../Defines.h"
