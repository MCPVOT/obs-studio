/* Precompiled header (BUILD.md) for the plugins/vlc-video unity groups
 * vlc-video.core. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* vlc-video-plugin.c */
#include <util/platform.h>
#include "vlc-video-plugin.h"
#include "vlc-video-host.h"

/* vlc-video-source.c */
#include "vlc-audio-order.h"
#include "vlc-drain-gate.h"
#include <media-io/video-frame.h>
#include <util/threading.h>
#include <util/dstr.h>
