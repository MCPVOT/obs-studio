/* Precompiled header (BUILD.md) for the plugins/mac-virtualcam/src/dal-plugin unity groups
 * dal-plugin.macos. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* CMSampleBufferUtils.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/CMSampleBufferUtils.h"

/* OBSDALDevice.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALDevice.h"

#include <IOKit/audio/IOAudioTypes.h>

#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALPlugIn.h"
#import "plugins/mac-virtualcam/src/dal-plugin/Logging.h"

/* OBSDALMachClient.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALMachClient.h"
#import "MachProtocol.h"

/* OBSDALObjectStore.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALObjectStore.h"

/* OBSDALPlugInInterface.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALPlugInInterface.h"

#import "plugins/mac-virtualcam/src/dal-plugin/OBSDALStream.h"

/* OBSDALPlugInMain.mm */
#import "plugins/mac-virtualcam/src/dal-plugin/Defines.h"

/* OBSDALStream.mm */

#import <AppKit/AppKit.h>
#import <mach/mach_time.h>
