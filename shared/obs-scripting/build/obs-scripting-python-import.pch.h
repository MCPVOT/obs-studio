/* Precompiled header (BUILD.md) for the shared/obs-scripting unity groups
 * obs-scripting.python-import. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-scripting-python-import.c */
#include <util/dstr.h>
#include <util/platform.h>

#define NO_REDEFS
#include "shared/obs-scripting/obs-scripting-python-import.h"

#ifdef _MSC_VER
#pragma warning(disable : 4152)
#endif

#ifdef _WIN32
#include <windows.h>
#include <io.h>
#define F_OK 0
#define access _access
#define VERSION_PATTERN "%d%d"
#define FILE_PATTERN "python%s.dll"
#define PATH_MAX MAX_PATH
#elif defined(__APPLE__)
#define VERSION_PATTERN "%d.%d"
#define FILE_PATTERN "Python.framework/Versions/%s/lib/libpython%s.dylib"
#endif
