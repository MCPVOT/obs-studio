/* Precompiled header (BUILD.md) for the plugins/coreaudio-encoder unity groups
 * coreaudio-encoder.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* encoder.cpp */
#include <util/dstr.hpp>
#include <obs-module.h>

#include <algorithm>
#include <cstdlib>
#include <initializer_list>
#include <memory>
#include <mutex>
#include <vector>

#ifndef _WIN32
#include <AudioToolbox/AudioToolbox.h>
#include <util/apple/cfstring-utils.h>
#endif

#define CA_LOG(level, format, ...) blog(level, "[CoreAudio encoder]: " format, ##__VA_ARGS__)
#define CA_LOG_ENCODER(format_name, encoder, level, format, ...) \
	blog(level, "[CoreAudio %s: '%s']: " format, format_name, obs_encoder_get_name(encoder), ##__VA_ARGS__)
#define CA_BLOG(level, format, ...) CA_LOG_ENCODER(ca->format_name, ca->encoder, level, format, ##__VA_ARGS__)
#define CA_CO_LOG(level, format, ...)                          \
	do {                                                   \
		if (ca)                                        \
			CA_BLOG(level, format, ##__VA_ARGS__); \
		else                                           \
			CA_LOG(level, format, ##__VA_ARGS__);  \
	} while (false)

#ifdef _WIN32
#include "plugins/coreaudio-encoder/windows-imports.h"
#endif
