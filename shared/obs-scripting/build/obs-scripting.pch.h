/* Precompiled header (BUILD.md) for the shared/obs-scripting unity groups
 * obs-scripting.core. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-scripting-logging.c */
#include "shared/obs-scripting/obs-scripting-internal.h"
#include <util/platform.h>

/* obs-scripting.c */
#include <obs.h>
#include <util/dstr.h>
#include <util/threading.h>
#include <util/deque.h>

#include "shared/obs-scripting/obs-scripting-callback.h"
#include "shared/obs-scripting/obs-scripting-python-host.h"
