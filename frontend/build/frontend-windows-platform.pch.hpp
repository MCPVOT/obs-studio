/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.windows-platform. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/platform-windows.cpp */
#include "frontend/utility/platform.hpp"

#include <OBSApp.hpp>

#include <QApplication>
#include <QFileInfo>
#include <QIcon>
#include <QSettings>
#include <QUrl>

#include <util/dstr.h>
#include <util/windows/ComPtr.hpp>
#include <util/windows/WinHandle.hpp>
#include <util/windows/win-version.h>

#include <Dwmapi.h>
#include <audiopolicy.h>
#include <mmdeviceapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <sstream>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wincodec.h>
#include <wincodecsdk.h>
#include <wrl/client.h>
#pragma comment(lib, "windowscodecs.lib")

#include "json11.hpp"
#include <util/platform.h>

#include "frontend/utility/OBSProxyStyle.hpp"

#include "frontend/widgets/OBSQTDisplay.hpp"

#include <QWindow>
