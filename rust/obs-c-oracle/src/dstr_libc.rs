//! `libobs/util/dstr-libc.c`, compiled unmodified as `oracle_*` by
//! `oracle/dstr_libc.c` over the oracle's dstr, utf8 and platform
//! conversions. Strings are the host `wchar_t`: `u16` on Windows, `i32` or
//! `u32` elsewhere (the oracle only passes pointers, so the element type is
//! the caller's).

use core::ffi::{c_char, c_int, c_void};

use crate::dstr::OracleDstr;

unsafe extern "C" {
    pub fn oracle_wstrcmpi(str1: *const c_void, str2: *const c_void) -> c_int;
    pub fn oracle_wstrcmp_n(str1: *const c_void, str2: *const c_void, n: usize) -> c_int;
    pub fn oracle_wstrcmpi_n(str1: *const c_void, str2: *const c_void, n: usize) -> c_int;
    pub fn oracle_wstrstri(str: *const c_void, find: *const c_void) -> *mut c_void;
    pub fn oracle_wcsdepad(str: *mut c_void) -> *mut c_void;
    pub fn oracle_dstr_from_mbs(dst: *mut OracleDstr, mbstr: *const c_char);
    pub fn oracle_dstr_to_mbs(str: *const OracleDstr) -> *mut c_char;
    pub fn oracle_dstr_to_wcs(str: *const OracleDstr) -> *mut c_void;
    pub fn oracle_dstr_from_wcs(dst: *mut OracleDstr, wstr: *const c_void);
    pub fn oracle_dstr_to_upper(str: *mut OracleDstr);
    pub fn oracle_dstr_to_lower(str: *mut OracleDstr);
}
