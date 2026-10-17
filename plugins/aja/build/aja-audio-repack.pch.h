/* Precompiled header (BUILD.md) for the plugins/aja unity groups
 * aja.audio-repack. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* audio-repack.c */
#include "plugins/aja/audio-repack.h"

#define USE_SIMD
#ifdef USE_SIMD
#include <util/sse-intrin.h>
#endif
