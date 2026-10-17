/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.macos-mm. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/system-info-macos.mm */
#include "frontend/utility/system-info.hpp"
#ifdef __aarch64__
#import <util/platform.h>
#import <Foundation/Foundation.h>
#import <Foundation/NSProcessInfo.h>
#import <sys/sysctl.h>
#import <sys/types.h>
#endif
