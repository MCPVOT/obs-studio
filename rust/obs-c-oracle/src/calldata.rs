//! `oracle_*` build of `libobs/callback/calldata.c.h`
//! (oracle/calldata.c). Pointers are the Rust `calldata` struct; the layout
//! test checks that struct against these `sizeof` / `offsetof` results.

use core::ffi::{c_char, c_void};

unsafe extern "C" {
    pub fn oracle_calldata_get_data(
        data: *const c_void,
        name: *const c_char,
        out: *mut c_void,
        size: usize,
    ) -> bool;
    pub fn oracle_calldata_set_data(
        data: *mut c_void,
        name: *const c_char,
        input: *const c_void,
        size: usize,
    );
    pub fn oracle_calldata_get_string(
        data: *const c_void,
        name: *const c_char,
        out: *mut *const c_char,
    ) -> bool;
    pub fn oracle_sizeof_calldata() -> usize;
    pub fn oracle_alignof_calldata() -> usize;
    pub fn oracle_offsetof_calldata_stack() -> usize;
    pub fn oracle_offsetof_calldata_size() -> usize;
    pub fn oracle_offsetof_calldata_capacity() -> usize;
    pub fn oracle_offsetof_calldata_fixed() -> usize;
}
