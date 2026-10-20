//! `oracle_*` build of `libobs/obs-missing-files.c.h`
//! (oracle/missing_files.c).

use core::ffi::{c_char, c_int, c_void};

unsafe extern "C" {
    pub fn oracle_obs_missing_files_create() -> *mut c_void;
    pub fn oracle_obs_missing_files_destroy(files: *mut c_void);
    pub fn oracle_obs_missing_files_add_file(files: *mut c_void, file: *mut c_void);
    pub fn oracle_obs_missing_files_count(files: *mut c_void) -> usize;
    pub fn oracle_obs_missing_files_get_file(files: *mut c_void, idx: c_int) -> *mut c_void;
    pub fn oracle_obs_missing_files_append(dst: *mut c_void, src: *mut c_void);
    pub fn oracle_obs_missing_file_create(
        path: *const c_char,
        callback: Option<unsafe extern "C" fn(*mut c_void, *const c_char, *mut c_void)>,
        src_type: c_int,
        src: *mut c_void,
        data: *mut c_void,
    ) -> *mut c_void;
    pub fn oracle_obs_missing_file_release(file: *mut c_void);
    pub fn oracle_obs_missing_file_destroy(file: *mut c_void);
    pub fn oracle_obs_missing_file_issue_callback(file: *mut c_void, new_path: *const c_char);
    pub fn oracle_obs_missing_file_get_path(file: *mut c_void) -> *const c_char;
    pub fn oracle_obs_missing_file_get_source_name(file: *mut c_void) -> *const c_char;
    pub fn oracle_missing_file_enum_source() -> c_int;
    pub fn oracle_missing_file_enum_script() -> c_int;
}
