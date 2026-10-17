/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.gpu-status. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/GpuStatus.cpp */
#include "frontend/utility/GpuStatus.hpp"
#include "frontend/utility/platform.hpp"

#ifdef OBS_GPU_STATUS
#include "frontend/utility/obs-gpu-status.h"
#endif

#include <OBSApp.hpp>
#include <obs.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cstring>
#include <mutex>
