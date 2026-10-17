/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.macos-arc-mm. Objective-C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* In the unity group the Objective-C++ files are included headers, where
 * clang checks nullability completeness (as mac-virtualcam's, #1212). */
#pragma clang diagnostic ignored "-Wnullability-completeness"

/* utility/CrashHandler_MacOS.mm (the group builds with -fmodules) */
@import Foundation;
#import "frontend/utility/CrashHandler.hpp"

/* utility/platform-osx.mm */
#import "frontend/utility/platform.hpp"

#import <OBSApp.hpp>

#import <util/dstr.h>
#import <util/threading.h>

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <dlfcn.h>

#if QT_VERSION >= QT_VERSION_CHECK(6, 11, 0) && QT_VERSION < QT_VERSION_CHECK(6, 12, 0)
#include <QtGui/private/qguiapplication_p.h>
#include <qpa/qplatformintegration.h>
#endif
