/* Precompiled header (BUILD.md) for the frontend/updater unity groups
 * updater.windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* hash.cpp */
#include "frontend/updater/updater.hpp"

#include <util/windows/WinHandle.hpp>
#include <vector>

/* helpers.cpp */
#include "frontend/updater/helpers.hpp"

/* http.cpp */

#include <algorithm>

/* updater.cpp */
#include "frontend/updater/manifest.hpp"

#include <psapi.h>
#include <SoftPub.h>
#include <WinTrust.h>

#include <util/windows/CoTaskMemPtr.hpp>

#include <exception>
#include <future>
#include <mutex>
#include <queue>
#include <string>
#include <string_view>
#include <unordered_set>
