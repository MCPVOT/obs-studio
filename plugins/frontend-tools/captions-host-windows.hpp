#pragma once

/* The Windows SDK headers the SAPI captions build on (#855), kept in a
 * Windows-named header so the captions headers need no host conditionals.
 * The captions tool is Windows only (CMakeLists.txt). */

#include <windows.h>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4996)
#endif

#include <sphelper.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif
