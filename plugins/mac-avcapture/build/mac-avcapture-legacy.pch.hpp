/* Precompiled header (BUILD.md) for the plugins/mac-avcapture unity groups
 * mac-avcapture.legacy-macos. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* legacy/av-capture.mm */
#import <AVFoundation/AVFoundation.h>
#import <CoreFoundation/CoreFoundation.h>
#import <CoreMedia/CoreMedia.h>
#import <CoreVideo/CoreVideo.h>
#import <CoreMediaIO/CMIOHardware.h>
#include <AvailabilityMacros.h>

#include <obs-module.h>
#include <obs.hpp>
#include <media-io/video-io.h>

#include <util/dstr.hpp>

#include <algorithm>
#include <initializer_list>
#include <cinttypes>
#include <limits>
#include <memory>
#include <vector>

#include "../legacy/left-right.hpp"
#include "../legacy/scope-guard.hpp"
