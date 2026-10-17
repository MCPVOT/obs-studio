/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.sparkle-arc-mm. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* In the unity group the Objective-C++ files are included headers, where
 * clang checks nullability completeness (as mac-virtualcam's, #1212). */
#pragma clang diagnostic ignored "-Wnullability-completeness"

/* utility/OBSSparkle.mm */
#import "frontend/utility/OBSSparkle.hpp"
#import "frontend/utility/OBSUpdateDelegate.h"

#import <util/platform.h>
