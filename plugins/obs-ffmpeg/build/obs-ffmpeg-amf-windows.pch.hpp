/* Precompiled header (BUILD.md) for the plugins/obs-ffmpeg unity groups
 * obs-ffmpeg.amf-windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* texture-amf.cpp */
#include <util/threading.h>
#include <opts-parser.h>
#include <obs-module.h>
#include <obs-avc.h>

#include <unordered_map>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <vector>
#include <mutex>
#include <deque>
#include <map>

#include <AMF/components/VideoEncoderHEVC.h>
#include <AMF/components/VideoEncoderVCE.h>
#include <AMF/components/VideoEncoderAV1.h>
#include <AMF/core/Factory.h>
#include <AMF/core/Trace.h>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <util/windows/device-enum.h>
#include <util/windows/HRError.hpp>
#include <util/windows/ComPtr.hpp>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/pipe.h>
#include <util/dstr.h>
