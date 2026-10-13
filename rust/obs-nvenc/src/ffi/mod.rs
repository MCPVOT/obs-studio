//! C ABI shim for the CUDA driver loader (obs-rust/obs-studio#524).
//!
//! `plugins/obs-nvenc/cuda-load-rust.c` replaces `init_cuda_internal`'s
//! `os_dlopen`/`os_dlsym` walk with [`obs_nvenc_cuda_fill`]. C keeps the
//! `CudaFunctions` layout: it passes the `{offsetof, name}` table it already
//! had, so no CUDA struct is mirrored here and the table cannot drift from
//! the header.

use std::ffi::{c_char, c_int, c_void};

use crate::{CudaDriver, LoadError};

/// The table was filled.
pub const OBS_NVENC_CUDA_OK: c_int = 0;
/// The driver library could not be loaded; C reports
/// `Loading CUDA library failed.`
pub const OBS_NVENC_CUDA_LIB_FAILED: c_int = 1;
/// A function is missing; C reports `Loading CUDA functions failed.`
pub const OBS_NVENC_CUDA_FUNCS_FAILED: c_int = 2;

/// One `cuda_functions[]` row: where in `CudaFunctions` the pointer goes and
/// the exported symbol it comes from.
#[repr(C)]
#[derive(Debug, Clone, Copy)]
pub struct ObsNvencCudaEntry {
    pub offset: isize,
    pub name: *const c_char,
}

/// Fills `table` (a `CudaFunctions`) from `loaded`. On failure returns the
/// `OBS_NVENC_CUDA_*` code and writes the log text (NUL-terminated,
/// truncated) to `err`.
///
/// # Safety
///
/// Each `entries[i].name` is a NUL-terminated string and
/// `table + entries[i].offset` is a writable pointer-sized slot.
pub unsafe fn fill_table(
    loaded: Result<&CudaDriver, &LoadError>,
    table: *mut u8,
    entries: &[ObsNvencCudaEntry],
    err: &mut [u8],
) -> c_int {
    let _ = (loaded, table, entries, err);
    OBS_NVENC_CUDA_LIB_FAILED
}

/// Loads the CUDA driver and fills `table`; see [`fill_table`].
///
/// # Safety
///
/// `entries` points to `count` rows as in [`fill_table`]; `err` is writable
/// for `err_len` bytes (or null with `err_len` 0).
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_nvenc_cuda_fill(
    table: *mut c_void,
    entries: *const ObsNvencCudaEntry,
    count: usize,
    err: *mut c_char,
    err_len: usize,
) -> c_int {
    let _ = (table, entries, count, err, err_len);
    OBS_NVENC_CUDA_LIB_FAILED
}
