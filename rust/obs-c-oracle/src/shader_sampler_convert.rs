//! Original sampler conversion and real-header layout/errno probes.
use core::ffi::{c_int, c_void};
unsafe extern "C" {
    pub fn oracle_shader_sampler_convert(sampler: *mut c_void, info: *mut c_void);
    pub fn oracle_shader_sampler_layout(index: usize) -> usize;
    pub fn oracle_shader_sampler_errno(
        convert: unsafe extern "C" fn(*mut c_void, *mut c_void),
        sampler: *mut c_void,
        info: *mut c_void,
    ) -> c_int;
}
