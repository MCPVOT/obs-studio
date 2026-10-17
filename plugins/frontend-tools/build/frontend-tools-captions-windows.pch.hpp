/* Precompiled header (BUILD.md) for the plugins/frontend-tools unity groups
 * frontend-tools.captions-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* captions.cpp */
#include <QMessageBox>
#include <QAction>

#include <windows.h>
#include <obs-frontend-api.h>
#include "plugins/frontend-tools/captions.hpp"
#include "plugins/frontend-tools/captions-handler.hpp"
#include "plugins/frontend-tools/tool-helpers.hpp"
#include <util/dstr.hpp>
#include <util/platform.h>
#include <util/windows/WinHandle.hpp>
#include <util/windows/ComPtr.hpp>
#include <obs-module.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include <sphelper.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#include <unordered_map>
#include <vector>
#include <string>
#include <thread>
#include <mutex>

#include "plugins/frontend-tools/captions-mssapi.hpp"
