/* Precompiled header (BUILD.md) for the plugins/obs-qsv11/obs-qsv-test unity groups
 * obs-qsv-test.windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-qsv-test.cpp */
#define ONEVPL_EXPERIMENTAL
#include <vpl/mfxstructures.h>
#include <vpl/mfxadapter.h>
#include <vpl/mfxvideo++.h>
#include "../common_utils.h"

#include <util/windows/ComPtr.hpp>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>
#include <string>
#include <map>
