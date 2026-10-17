/* Precompiled header (BUILD.md) for the plugins/decklink unity groups
 * decklink.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* DecklinkBase.cpp */
#include "plugins/decklink/DecklinkBase.h"

/* DecklinkInput.cpp */
#include "plugins/decklink/DecklinkInput.hpp"

#include <util/threading.h>

/* DecklinkOutput.cpp */
#include "plugins/decklink/DecklinkOutput.hpp"

/* OBSVideoFrame.cpp */
#include "plugins/decklink/OBSVideoFrame.h"

/* decklink-device-discovery.cpp */
#include "plugins/decklink/decklink-device-discovery.hpp"
#include "plugins/decklink/decklink-device.hpp"

/* decklink-device-instance.cpp */
#include "plugins/decklink/decklink-device-instance.hpp"
#include "plugins/decklink/audio-repack.hpp"

#include <util/platform.h>
#include <util/util_uint64.h>

#include <sstream>
#include <iomanip>
#include <algorithm>

#include <caption/caption.h>
#include <util/bitstream.h>

/* decklink-device-mode.cpp */
#include "plugins/decklink/decklink-device-mode.hpp"

/* decklink-devices.cpp */
#include "plugins/decklink/decklink-devices.hpp"

/* decklink-output.cpp */
#include <obs-module.h>
#include <obs-avc.h>

#include "plugins/decklink/const.h"

#include <media-io/video-scaler.h>

/* decklink-source.cpp */

#include "plugins/decklink/util.hpp"
