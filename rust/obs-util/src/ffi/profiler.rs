//! C ABI shims for `libobs/util/profiler.c`.
//!
//! `profile_store_name` stays in `util/profiler-variadic.c`. Print and the
//! CSV dump stay in `util/profiler-io.c`. Snapshots are `bmalloc` / `DARRAY`
//! values so `profile_snapshot_free` can release them.

use core::ffi::c_char;
use core::mem::size_of;
use core::ptr;

use super::bmem::bmalloc;
use super::darray::darray_resize;
use super::profiler_snapshot::{
    Darray, profiler_snapshot, profiler_snapshot_entry, profiler_time_entry,
};
use crate::profiler::{self as prof, NameStore, ProfileName, Snapshot, SnapshotEntry, TimeEntry};

unsafe extern "C" {
    fn os_gettime_ns() -> u64;
}

/// # Safety
///
/// No pointer arguments.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_start() {
    prof::profiler_start();
}

/// # Safety
///
/// No pointer arguments.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_stop() {
    prof::profiler_stop();
}

/// # Safety
///
/// No pointer arguments. Drops recorded roots.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_free() {
    prof::profiler_free();
}

/// # Safety
///
/// No pointer arguments.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_reenable_thread() {
    prof::profile_reenable_thread();
}

/// # Safety
///
/// `name` follows the `profile_register_root` contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_register_root(
    name: *const c_char,
    expected_time_between_calls: u64,
) {
    prof::profile_register_root(ProfileName::new(name), expected_time_between_calls);
}

/// # Safety
///
/// `name` follows the `profile_start` contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_start(name: *const c_char) {
    if !prof::thread_enabled() {
        return;
    }
    // SAFETY: libobs clock. Read only when the thread is enabled, as in C.
    let now = unsafe { os_gettime_ns() };
    prof::profile_start(ProfileName::new(name), now);
}

/// # Safety
///
/// `name` follows the `profile_end` contract.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_end(name: *const c_char) {
    prof::profile_end(ProfileName::new(name), &mut || unsafe { os_gettime_ns() });
}

/// # Safety
///
/// The result is freed with `profiler_name_store_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_name_store_create() -> *mut NameStore {
    prof::name_store_create()
}

/// # Safety
///
/// `store` is null or a pointer from [`profiler_name_store_create`].
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_name_store_free(store: *mut NameStore) {
    unsafe { prof::name_store_free(store) }
}

/// # Safety
///
/// `store` is a live name store. `owned` is null or a `bmalloc` string this
/// function frees.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_name_store_push(
    store: *mut NameStore,
    owned: *mut c_char,
) -> *const c_char {
    unsafe { (*store).push_owned(owned) }
}

/// # Safety
///
/// `dst` is a zeroed entry darray.
unsafe fn fill_times(dst: &mut Darray<profiler_time_entry>, times: &[TimeEntry]) {
    if times.is_empty() {
        return;
    }
    // SAFETY: `dst` is a valid darray of time entries.
    unsafe { darray_resize(size_of::<profiler_time_entry>(), dst.as_raw(), times.len()) };
    for (i, time) in times.iter().enumerate() {
        // SAFETY: the resize wrote `times.len()` slots.
        unsafe {
            *dst.array.add(i) = profiler_time_entry {
                time_delta: time.time_delta,
                count: time.count,
            };
        }
    }
}

/// # Safety
///
/// `dst` is a zeroed snapshot entry.
unsafe fn fill_entry(dst: &mut profiler_snapshot_entry, src: &SnapshotEntry) {
    dst.name = src.name.as_ptr();
    dst.min_time = src.min_time;
    dst.max_time = src.max_time;
    dst.overall_count = src.overall_count;
    dst.expected_time_between_calls = src.expected_time_between_calls;
    dst.min_time_between_calls = src.min_time_between_calls;
    dst.max_time_between_calls = src.max_time_between_calls;
    dst.overall_between_calls_count = src.overall_between_calls_count;
    // SAFETY: the time and child arrays were zeroed with the entry.
    unsafe {
        fill_times(&mut dst.times, &src.times);
        fill_times(&mut dst.times_between_calls, &src.times_between_calls);
        if src.children.is_empty() {
            return;
        }
        darray_resize(
            size_of::<profiler_snapshot_entry>(),
            dst.children.as_raw(),
            src.children.len(),
        );
        for (i, child) in src.children.iter().enumerate() {
            fill_entry(&mut *dst.children.array.add(i), child);
        }
    }
}

unsafe fn write_snapshot(src: &Snapshot) -> *mut profiler_snapshot {
    // SAFETY: `bmalloc` returns the libobs heap, which `profile_snapshot_free` releases.
    let raw = unsafe { bmalloc(size_of::<profiler_snapshot>()).cast::<profiler_snapshot>() };
    unsafe { ptr::write_bytes(raw.cast::<u8>(), 0, size_of::<profiler_snapshot>()) };
    if src.roots.is_empty() {
        return raw;
    }
    // SAFETY: `raw` points at a zeroed snapshot.
    unsafe {
        let roots = &mut (*raw).roots;
        darray_resize(
            size_of::<profiler_snapshot_entry>(),
            roots.as_raw(),
            src.roots.len(),
        );
        for (i, root) in src.roots.iter().enumerate() {
            fill_entry(&mut *roots.array.add(i), root);
        }
    }
    raw
}

/// # Safety
///
/// The result is freed with `profile_snapshot_free`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_snapshot_create() -> *mut profiler_snapshot {
    let snap = prof::profile_snapshot_create();
    unsafe { write_snapshot(&snap) }
}
