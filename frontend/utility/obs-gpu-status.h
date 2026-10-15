#pragma once

/* C ABI of rust/obs-gpu-status (src/ffi/mod.rs), linked into the frontend
 * only when ENABLE_RUST_LIBOBS is on (obs-rust/obs-studio#846). */

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Classify `adapter` (NULL when graphics init failed) against the GPUs
 * detected on the system (probed once per process);
 * interval_ns == 0 means no frame timings. Writes 0 (green),
 * 1 (yellow) or 2 (red) to `state` when it is not NULL. Returns the
 * gpuAcceleration JSON object; free it with obs_gpu_status_free. */
char *obs_gpu_status_report(const char *adapter, uint64_t median_frame_ns, uint64_t interval_ns, int *state);

void obs_gpu_status_free(char *json);

#ifdef __cplusplus
}
#endif
