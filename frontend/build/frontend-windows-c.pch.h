/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.windows-c. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/win-dll-blocklist.c */
#include <Windows.h>
#include <psapi.h>
#include <stdint.h>
#include <stdbool.h>
#include <inttypes.h>
#include "detours.h"
#include "obs.h"
