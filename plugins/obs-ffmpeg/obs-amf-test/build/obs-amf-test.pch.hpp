/* Precompiled header (BUILD.md) for the plugins/obs-ffmpeg/obs-amf-test unity groups
 * obs-amf-test.windows. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-amf-test.cpp */
#include <AMF/core/Factory.h>
#include <AMF/core/Trace.h>
#include <AMF/components/VideoEncoderVCE.h>
#include <AMF/components/VideoEncoderHEVC.h>
#include <AMF/components/VideoEncoderAV1.h>

#include <util/windows/ComPtr.hpp>

#include <dxgi.h>
#include <d3d11.h>
#include <d3d11_1.h>

#include <vector>
#include <string>
#include <map>
