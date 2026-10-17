/* Precompiled header (BUILD.md) for the plugins/mac-virtualcam/src/dal-plugin unity groups
 * dal-plugin.macos. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* CMSampleBufferUtils.mm */
#import "../CMSampleBufferUtils.h"

/* OBSDALDevice.mm */
#import "../OBSDALDevice.h"

#include <IOKit/audio/IOAudioTypes.h>

#import "../OBSDALPlugIn.h"
#import "../Logging.h"

/* OBSDALMachClient.mm */
#import "../OBSDALMachClient.h"
#import "MachProtocol.h"

/* OBSDALObjectStore.mm */
#import "../OBSDALObjectStore.h"

/* OBSDALPlugInInterface.mm */
#import "../OBSDALPlugInInterface.h"

#import "../OBSDALStream.h"

/* OBSDALPlugInMain.mm */
#import "../Defines.h"

/* OBSDALStream.mm */

#import <AppKit/AppKit.h>
#import <mach/mach_time.h>
