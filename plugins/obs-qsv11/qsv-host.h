#pragma once

/* obs-qsv11's host interface (obs-rust/obs-studio#860): what differs between
 * Windows (Direct3D 11) and Linux (VA-API), so the shared sources need no
 * platform conditionals. The C part is implemented in qsv-host-<os>.c.h, the
 * C++ part in common_utils_<os>.cpp.hpp, each in its OS's unity groups,
 * picked by CMake (BUILD.md). */

#include <stdbool.h>

struct encoder_texture;

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the texture handed to encode_texture2 can be encoded: a shared
 * handle on Windows, the two NV12/P010 plane textures on Linux. */
bool qsv_host_texture_valid(const struct encoder_texture *tex);

#ifdef __cplusplus
}

#include <vector>

#include <vpl/mfxdispatcher.h>
#include <vpl/mfxstructures.h>

#include "plugins/obs-qsv11/QSV_Encoder.h"

/* Adds the host's acceleration mode filter to the session config and sets
 * whether the encoder uses D3D11 and texture allocation (Windows only). */
void qsv_host_select_accel_mode(mfxConfig cfg, bool *use_d3d11, bool *use_tex_alloc);

/* Adds the video signal info buffer where the host sends one (Windows). */
void qsv_host_add_video_signal_info(mfxExtVideoSignalInfo *info, const qsv_param_t *pParams,
				    std::vector<mfxExtBuffer *> &extendedBuffers);
#endif
