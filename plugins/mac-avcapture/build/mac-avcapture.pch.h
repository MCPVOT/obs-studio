/* Precompiled header (BUILD.md) for the plugins/mac-avcapture unity groups
 * mac-avcapture.objc-macos. Objective-C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* AVCaptureDeviceFormat+OBSListable declares its category properties
 * read-write and implements only their getters (the setters go through
 * associated objects in the getters). Compiled as its own main file this was
 * never reported; in the unity group clang reports it under -Werror. */
#pragma clang diagnostic ignored "-Wobjc-property-implementation"

/* AVCaptureDeviceFormat+OBSListable.m */
#import "plugins/mac-avcapture/OBSAVCapture.h"

/* OBSAVCapture.m */
#import "plugins/mac-avcapture/AVCaptureDeviceFormat+OBSListable.h"
#import "plugins/mac-avcapture/avcapture-audio-planes.h"

/* OBSAVCapturePresetInfo.m */
#import <OBSAVCapturePresetInfo.h>

/* plugin-main.m */
#import "plugins/mac-avcapture/plugin-main.h"

/* plugin-properties.m */
#import "plugins/mac-avcapture/plugin-properties.h"
