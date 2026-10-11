//! Safe core for `libobs/util/profiler.c`.
//!
//! The C ABI shim lives in [`crate::ffi::profiler`]. Names are keyed by
//! pointer identity, as in C. Timestamps are passed in so tests can script
//! `os_gettime_ns`; the shim reads the real clock.

use core::ffi::c_char;
use std::sync::Mutex;

/// A profile name. Equality is the pointer address, not the bytes.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, Hash)]
pub struct ProfileName(usize);

impl ProfileName {
    /// The address of a string that lives for the whole process.
    pub fn from_static(name: &'static str) -> Self {
        Self(name.as_ptr() as usize)
    }

    /// # Safety
    ///
    /// `name` must stay valid for as long as the profiler stores it, which
    /// is what `profile_start` requires of its caller.
    pub fn new(name: *const c_char) -> Self {
        Self(name as usize)
    }

    pub fn as_ptr(self) -> *const c_char {
        self.0 as *const c_char
    }

    pub fn is_null(self) -> bool {
        self.0 == 0
    }
}

/// One `(time_delta, count)` bucket from a snapshot.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq)]
pub struct TimeEntry {
    pub time_delta: u64,
    pub count: u64,
}

/// One node of a snapshot, in registration / first-seen order.
#[derive(Clone, Debug, Default)]
pub struct SnapshotEntry {
    pub name: ProfileName,
    pub times: Vec<TimeEntry>,
    pub min_time: u64,
    pub max_time: u64,
    pub overall_count: u64,
    pub times_between_calls: Vec<TimeEntry>,
    pub expected_time_between_calls: u64,
    pub min_time_between_calls: u64,
    pub max_time_between_calls: u64,
    pub overall_between_calls_count: u64,
    pub children: Vec<SnapshotEntry>,
}

/// Roots of a profiler snapshot.
#[derive(Clone, Debug, Default)]
pub struct Snapshot {
    pub roots: Vec<SnapshotEntry>,
}

/// Owned copies of formatted profile names.
///
/// Each `store` returns a distinct pointer, including for equal bytes. The
/// pointers stay valid until the store is dropped.
#[derive(Default)]
pub struct NameStore {
    names: Mutex<Vec<usize>>,
}

impl NameStore {
    pub fn new() -> Self {
        Self {
            names: Mutex::new(Vec::new()),
        }
    }

    /// Copies `text` (no interior NUL) and returns the stored C string.
    ///
    /// RED stub: returns null so the Tier 1 name-store assertions fail.
    pub fn store(&self, text: &str) -> *const c_char {
        let _ = text;
        self.names.lock().unwrap().clear();
        core::ptr::null()
    }

    /// Takes ownership of a `bmalloc`'d NUL-terminated buffer.
    pub fn push_owned(&self, owned: *mut c_char) -> *const c_char {
        let _ = owned;
        self.names.lock().unwrap().clear();
        core::ptr::null()
    }
}

/// Frees a store from the C ABI. Null is a no-op.
///
/// # Safety
///
/// `store` is null or a pointer returned by [`name_store_create`].
pub unsafe fn name_store_free(store: *mut NameStore) {
    if store.is_null() {
        return;
    }
    // SAFETY: the caller transferred a box from `name_store_create`.
    drop(unsafe { Box::from_raw(store) });
}

/// Boxes a store for the C ABI.
pub fn name_store_create() -> *mut NameStore {
    Box::into_raw(Box::new(NameStore::new()))
}

pub fn profiler_start() {}

pub fn profiler_stop() {}

pub fn profiler_free() {}

pub fn profile_reenable_thread() {}

pub fn thread_enabled() -> bool {
    true
}

pub fn profile_register_root(name: ProfileName, expected_time_between_calls_ns: u64) {
    let _ = (name, expected_time_between_calls_ns);
}

pub fn profile_start(name: ProfileName, now_ns: u64) {
    let _ = (name, now_ns);
}

pub fn profile_end(name: ProfileName, now_ns: u64) {
    let _ = (name, now_ns);
}

/// RED stub: an empty snapshot, so lifecycle counts fail.
pub fn profile_snapshot_create() -> Snapshot {
    Snapshot::default()
}
