/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.crypto-macos-mm. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* In the unity group the Objective-C++ files are included headers, where
 * clang checks nullability completeness (as mac-virtualcam's, #1212). */
#pragma clang diagnostic ignored "-Wnullability-completeness"

/* utility/crypto-helpers-mac.mm */
#include "frontend/utility/crypto-helpers.hpp"

#import <Foundation/Foundation.h>
#import <Security/Security.h>
#import <Security/SecKey.h>
