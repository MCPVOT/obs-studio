//! Which source type copies a name, frees it, and runs the replace callback.
//!
//! `OBS_MISSING_FILE_SOURCE` is 0 and `OBS_MISSING_FILE_SCRIPT` is 1, as in
//! `obs-missing-files.h`. Any other `src_type` stores the callback and does
//! nothing with it.

/// `OBS_MISSING_FILE_SOURCE`.
pub const SOURCE: i32 = 0;

/// `OBS_MISSING_FILE_SCRIPT`.
pub const SCRIPT: i32 = 1;

/// Source files copy `obs_source_get_name` at create.
pub fn copies_source_name(src_type: i32) -> bool {
    src_type == SOURCE
}

/// `obs_missing_file_destroy` frees `src_name` only for a source file.
pub fn frees_source_name(src_type: i32) -> bool {
    src_type == SOURCE
}

/// `obs_missing_file_issue_callback` calls replace only for a source file.
pub fn calls_replace(src_type: i32) -> bool {
    src_type == SOURCE
}
