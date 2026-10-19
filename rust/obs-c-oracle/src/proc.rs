//! `oracle_*` build of `libobs/callback/proc.c.h` (oracle/proc.c).

use core::ffi::{c_char, c_void};

unsafe extern "C" {
    pub fn oracle_proc_handler_create() -> *mut c_void;
    pub fn oracle_proc_handler_destroy(handler: *mut c_void);
    pub fn oracle_proc_handler_add(
        handler: *mut c_void,
        decl_string: *const c_char,
        proc: Option<unsafe extern "C" fn(*mut c_void, *mut c_void)>,
        data: *mut c_void,
    );
    pub fn oracle_proc_handler_call(
        handler: *mut c_void,
        name: *const c_char,
        params: *mut c_void,
    ) -> bool;
}
