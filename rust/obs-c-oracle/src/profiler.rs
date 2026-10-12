//! `util/profiler.c`: the recording engine.
//!
//! Print and CSV stay in `profiler-io.c`. The scripted clock is
//! `oracle_profiler_gettime_ns` in `oracle/profiler.c`.

use core::ffi::c_char;

use crate::profiler_snapshot::OracleSnapshot;

unsafe extern "C" {
    pub fn oracle_profiler_set_clock(times: *const u64, len: usize);
    pub fn oracle_profiler_reset();
    pub fn oracle_profiler_start();
    pub fn oracle_profiler_stop();
    pub fn oracle_profile_reenable_thread();
    pub fn oracle_profile_register_root(name: *const c_char, expected_time_between_calls: u64);
    pub fn oracle_profile_start(name: *const c_char);
    pub fn oracle_profile_end(name: *const c_char);
    pub fn oracle_profiler_free();
    pub fn oracle_profile_snapshot_create() -> *mut OracleSnapshot;
    pub fn oracle_profiler_name_store_create() -> *mut core::ffi::c_void;
    pub fn oracle_profiler_name_store_free(store: *mut core::ffi::c_void);
    pub fn oracle_profile_store_name(
        store: *mut core::ffi::c_void,
        format: *const c_char,
        ...
    ) -> *const c_char;
}
