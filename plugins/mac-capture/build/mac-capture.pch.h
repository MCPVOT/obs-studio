/* Precompiled header (BUILD.md) for the plugins/mac-capture unity groups
 * mac-capture.core-macos. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* audio-device-enum.c */
#include <CoreFoundation/CFString.h>
#include <CoreAudio/CoreAudio.h>

#include <util/apple/cfstring-utils.h>

#include "../audio-device-enum.h"

/* mac-audio.c */
#include <AudioUnit/AudioUnit.h>
#include <unistd.h>
#include <errno.h>

#include <obs-module.h>
#include <mach/mach_time.h>
#include <util/threading.h>
#include <util/c99defs.h>
