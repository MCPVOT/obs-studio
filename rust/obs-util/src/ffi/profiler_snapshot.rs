//! C ABI shims for `libobs/util/profiler-snapshot.c`: the snapshot
//! accessors and `profile_snapshot_free`.
//!
//! `ffi::profiler` builds the snapshot with `bmalloc` and the `DARRAY`
//! layout. These shims read it in place through `#[repr(C)]`
//! mirrors of the private structs in `profiler-snapshot.h`, and free it with
//! the same `bfree` / `da_free` / `da_erase` steps as the C file.

use core::ffi::{c_char, c_void};
use core::mem::size_of;
use core::ptr;

use super::darray::{bfree, darray, darray_erase, darray_free};
use crate::profiler_snapshot::{EntryList, FilterStep, enumerate, filter};

/// `DARRAY(T)`: the anonymous struct in the `darray.h` union, laid out like
/// `struct darray` with a typed `array`.
#[repr(C)]
#[derive(Debug)]
pub struct Darray<T> {
    pub array: *mut T,
    pub num: usize,
    pub capacity: usize,
}

impl<T> Darray<T> {
    pub(crate) fn as_raw(&mut self) -> *mut darray {
        (self as *mut Self).cast()
    }
}

/// `struct profiler_time_entry` from `profiler.h`.
#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct profiler_time_entry {
    pub time_delta: u64,
    pub count: u64,
}

/// `profiler_time_entries_t`.
#[allow(non_camel_case_types)]
pub type profiler_time_entries_t = Darray<profiler_time_entry>;

/// `struct profiler_snapshot_entry` from `profiler-snapshot.h`.
#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Debug)]
pub struct profiler_snapshot_entry {
    pub name: *const c_char,
    pub times: profiler_time_entries_t,
    pub min_time: u64,
    pub max_time: u64,
    pub overall_count: u64,
    pub times_between_calls: profiler_time_entries_t,
    pub expected_time_between_calls: u64,
    pub min_time_between_calls: u64,
    pub max_time_between_calls: u64,
    pub overall_between_calls_count: u64,
    pub children: Darray<profiler_snapshot_entry>,
}

/// `struct profiler_snapshot` from `profiler-snapshot.h`.
#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Debug)]
pub struct profiler_snapshot {
    pub roots: Darray<profiler_snapshot_entry>,
}

/// `profiler_entry_enum_func`.
pub type EntryEnumFunc =
    Option<unsafe extern "C" fn(context: *mut c_void, entry: *mut profiler_snapshot_entry) -> bool>;

/// `profiler_name_filter_func`.
pub type NameFilterFunc =
    Option<unsafe extern "C" fn(data: *mut c_void, name: *const c_char, remove: *mut bool) -> bool>;

/// `free_snapshot_entry`: children first, then the entry's own arrays, in
/// the C order.
///
/// # Safety
///
/// `entry` is a valid snapshot entry whose arrays came from `bmalloc`.
unsafe fn free_snapshot_entry(entry: &mut profiler_snapshot_entry) {
    // SAFETY: `children.array` holds `children.num` valid entries.
    unsafe {
        for i in 0..entry.children.num {
            free_snapshot_entry(&mut *entry.children.array.add(i));
        }
        darray_free(entry.children.as_raw());
        darray_free(entry.times_between_calls.as_raw());
        darray_free(entry.times.as_raw());
    }
}

/// A C darray of snapshot entries, seen through [`EntryList`].
struct Entries<'a>(&'a mut Darray<profiler_snapshot_entry>);

impl EntryList for Entries<'_> {
    type Entry = profiler_snapshot_entry;

    fn len(&self) -> usize {
        self.0.num
    }

    fn entry(&mut self, idx: usize) -> &mut profiler_snapshot_entry {
        // SAFETY: the core only asks for `idx < num`, and `array` holds
        // `num` valid entries.
        unsafe { &mut *self.0.array.add(idx) }
    }

    fn remove(&mut self, idx: usize) {
        // SAFETY: `idx < num`; the entry is freed, then the C `da_erase`
        // closes the gap.
        unsafe {
            free_snapshot_entry(&mut *self.0.array.add(idx));
            darray_erase(size_of::<profiler_snapshot_entry>(), self.0.as_raw(), idx);
        }
    }
}

/// Frees a snapshot and every entry in it. NULL is a no-op.
///
/// # Safety
///
/// `snap` is NULL or a snapshot from `profile_snapshot_create` that has not
/// been freed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profile_snapshot_free(snap: *mut profiler_snapshot) {
    if snap.is_null() {
        return;
    }
    // SAFETY: the caller guarantees `snap` is a live snapshot.
    unsafe {
        let roots = &mut (*snap).roots;
        for i in 0..roots.num {
            free_snapshot_entry(&mut *roots.array.add(i));
        }
        darray_free(roots.as_raw());
        bfree(snap.cast());
    }
}

/// # Safety
///
/// `snap` is NULL or a live snapshot.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_num_roots(snap: *mut profiler_snapshot) -> usize {
    // SAFETY: the caller guarantees `snap` is NULL or live.
    unsafe { snap.as_ref() }.map_or(0, |s| s.roots.num)
}

/// Calls `func` on each root until it returns false.
///
/// A NULL `func` with roots to visit does nothing; C would call through it.
///
/// # Safety
///
/// `snap` is NULL or a live snapshot. `func` follows the
/// `profiler_entry_enum_func` contract for `context`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_enumerate_roots(
    snap: *mut profiler_snapshot,
    func: EntryEnumFunc,
    context: *mut c_void,
) {
    // SAFETY: the caller guarantees `snap` is NULL or live.
    let (Some(snap), Some(func)) = (unsafe { snap.as_mut() }, func) else {
        return;
    };
    // SAFETY: `func` is the caller's callback; each entry is live.
    enumerate(&mut Entries(&mut snap.roots), |e| unsafe {
        func(context, e)
    });
}

/// Asks `func` about each root and removes those it marks.
///
/// A NULL `snap` or `func` does nothing; C would dereference them.
///
/// # Safety
///
/// `snap` is NULL or a live snapshot. `func` follows the
/// `profiler_name_filter_func` contract for `data`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_filter_roots(
    snap: *mut profiler_snapshot,
    func: NameFilterFunc,
    data: *mut c_void,
) {
    // SAFETY: the caller guarantees `snap` is NULL or live.
    let (Some(snap), Some(func)) = (unsafe { snap.as_mut() }, func) else {
        return;
    };
    filter(&mut Entries(&mut snap.roots), |e| {
        // C: bool remove = false; bool res = func(data, name, &remove);
        let mut remove = false;
        // SAFETY: `func` is the caller's callback; `remove` is a live local.
        let keep_going = unsafe { func(data, e.name, &mut remove) };
        FilterStep { keep_going, remove }
    });
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_num_children(
    entry: *mut profiler_snapshot_entry,
) -> usize {
    // SAFETY: the caller guarantees `entry` is NULL or live.
    unsafe { entry.as_ref() }.map_or(0, |e| e.children.num)
}

/// Calls `func` on each child until it returns false.
///
/// A NULL `func` with children to visit does nothing; C would call through
/// it.
///
/// # Safety
///
/// `entry` is NULL or a live snapshot entry. `func` follows the
/// `profiler_entry_enum_func` contract for `context`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_enumerate_children(
    entry: *mut profiler_snapshot_entry,
    func: EntryEnumFunc,
    context: *mut c_void,
) {
    // SAFETY: the caller guarantees `entry` is NULL or live.
    let (Some(entry), Some(func)) = (unsafe { entry.as_mut() }, func) else {
        return;
    };
    // SAFETY: `func` is the caller's callback; each child is live.
    enumerate(&mut Entries(&mut entry.children), |e| unsafe {
        func(context, e)
    });
}

/// Reads one field of a possibly-NULL entry, with `default` for NULL, as
/// every `entry ? entry->x : 0` getter in the C file does.
///
/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
unsafe fn field<T>(
    entry: *mut profiler_snapshot_entry,
    default: T,
    get: impl FnOnce(&mut profiler_snapshot_entry) -> T,
) -> T {
    // SAFETY: the caller guarantees `entry` is NULL or live.
    unsafe { entry.as_mut() }.map_or(default, get)
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_name(
    entry: *mut profiler_snapshot_entry,
) -> *const c_char {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, ptr::null(), |e| e.name) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_times(
    entry: *mut profiler_snapshot_entry,
) -> *mut profiler_time_entries_t {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, ptr::null_mut(), |e| &mut e.times) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_overall_count(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.overall_count) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_min_time(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.min_time) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_max_time(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.max_time) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_times_between_calls(
    entry: *mut profiler_snapshot_entry,
) -> *mut profiler_time_entries_t {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, ptr::null_mut(), |e| &mut e.times_between_calls) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_expected_time_between_calls(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.expected_time_between_calls) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_min_time_between_calls(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.min_time_between_calls) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_max_time_between_calls(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.max_time_between_calls) }
}

/// # Safety
///
/// `entry` is NULL or a live snapshot entry.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn profiler_snapshot_entry_overall_between_calls_count(
    entry: *mut profiler_snapshot_entry,
) -> u64 {
    // SAFETY: forwarded from the caller.
    unsafe { field(entry, 0, |e| e.overall_between_calls_count) }
}
