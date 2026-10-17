/* Precompiled header (BUILD.md) for the plugins/win-capture/inject-helper unity groups
 * inject-helper.inject-helper-windows. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* inject-helper.c */
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>
#include <stdbool.h>
#ifdef OBS_LEGACY
#include "../../../libobs/util/windows/obfuscate.h"
#include "../inject-library.h"
#else
#include <util/windows/obfuscate.h>
#include <inject-library.h>
#endif
