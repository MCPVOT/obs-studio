/* Precompiled header (BUILD.md) for the libobs unity group
 * libobs.ported-audio-controls. The C file reads obs_source.sample_info,
 * so this is obs-internal.h plus the audio-controls slice of libobs.pch.h.
 * The implementation file includes this header and nothing else. */

#pragma once

#include <math.h>
#include <stdio.h>

#include "obs-internal.h"
#include "util/sse-intrin.h"
#include "util/bmem.h"
#include "media-io/audio-math.h"
#include "obs-audio-controls.h"
