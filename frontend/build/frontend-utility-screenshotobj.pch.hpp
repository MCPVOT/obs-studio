/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.utility-screenshotobj. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/ScreenshotObj.cpp */
#include "frontend/utility/ScreenshotObj.hpp"
#include "frontend/utility/screenshot-render.hpp"

#include <utility/output-filename-reserve.h>

#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

#ifdef _WIN32
#include <wincodec.h>
#include <wincodecsdk.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")
#endif
