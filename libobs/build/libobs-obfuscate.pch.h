/* Precompiled header (BUILD.md) for the libobs unity groups
 * libobs.obfuscate-windows. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* util/windows/obfuscate.c */
#ifdef _MSC_VER
#pragma warning(disable : 4152) /* casting func ptr to void */
#endif

#include <stdbool.h>
#include <windows.h>
#include "libobs/util/windows/obfuscate.h"
