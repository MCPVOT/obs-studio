/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.app-obs-main. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-main.cpp */
#include <OBSApp.hpp>

#include <components/VolumeAccessibleInterface.hpp>
#ifdef __APPLE__
#include <dialogs/OBSPermissions.hpp>
#endif
#include <utility/BaseLexer.hpp>
#include <utility/GpuStatus.hpp>
#include <utility/OBSTranslator.hpp>
#include <utility/platform.hpp>

#include <qt-wrappers.hpp>
#include <util/platform.h>
#include <util/profiler.h>
#include <util/util.hpp>
#ifdef _WIN32
#include <util/windows/win-version.h>
#endif

#include <QFontDatabase>
#include <QProcess>
#include <QPushButton>
#include <curl/curl.h>

#include <fstream>
#include <iostream>
#include <sstream>
#ifdef _WIN32
#include <shellapi.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <signal.h>
#endif
