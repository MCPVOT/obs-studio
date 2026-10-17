/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.utility-output-filename-reserve. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/output-filename-reserve.c */
#include "frontend/utility/output-filename-reserve.h"

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>
