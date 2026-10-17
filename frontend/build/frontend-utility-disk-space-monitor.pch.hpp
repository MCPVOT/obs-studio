/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.utility-disk-space-monitor. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/disk-space-monitor.cpp */
#include "frontend/utility/disk-space-monitor.hpp"

#include <util/platform.h>

#include <chrono>
#include <memory>
#include <condition_variable>
#include <mutex>
