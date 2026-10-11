//! Safe core of `libobs/util/profiler-snapshot.c`: walking and filtering
//! the entries of a profiler snapshot.
//!
//! Snapshots are built by `profiler.c`'s Rust port (`crate::profiler`). The C ABI shim in
//! [`crate::ffi::profiler_snapshot`] reads them in place; this module holds
//! the loop logic, written against [`EntryList`] so it runs the same over
//! the C darray and over a `Vec` in tests.

/// A list of snapshot entries that can be walked and shrunk in place.
pub trait EntryList {
    type Entry;

    /// Current number of entries. Read again after every callback, as the
    /// C loops re-read `num`.
    fn len(&self) -> usize;

    fn is_empty(&self) -> bool {
        self.len() == 0
    }

    /// The entry at `idx < len()`.
    fn entry(&mut self, idx: usize) -> &mut Self::Entry;

    /// Frees the entry at `idx < len()` and closes the gap, keeping the
    /// order of the rest (`free_snapshot_entry` + `da_erase`).
    fn remove(&mut self, idx: usize);
}

impl<T> EntryList for Vec<T> {
    type Entry = T;

    fn len(&self) -> usize {
        Vec::len(self)
    }

    fn entry(&mut self, idx: usize) -> &mut T {
        &mut self[idx]
    }

    fn remove(&mut self, idx: usize) {
        Vec::remove(self, idx);
    }
}

/// What a filter callback decided for one entry
/// (`profiler_name_filter_func`'s return value and `*remove`).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FilterStep {
    /// The callback's return value: `false` stops the walk.
    pub keep_going: bool,
    /// `*remove`: drop this entry.
    pub remove: bool,
}

/// Calls `f` on each entry in order until it returns `false`
/// (`profiler_snapshot_enumerate_roots` / `_children`).
pub fn enumerate<L: EntryList>(list: &mut L, mut f: impl FnMut(&mut L::Entry) -> bool) {
    let mut i = 0;
    while i < list.len() {
        if !f(list.entry(i)) {
            break;
        }
        i += 1;
    }
}

/// Asks `f` about each entry in order and removes the ones it marks
/// (`profiler_snapshot_filter_roots`).
///
/// A removed entry is dropped before `keep_going` is honored, so the last
/// entry the callback sees can still be removed. The index only advances
/// past entries that stay, so every entry is seen once.
pub fn filter<L: EntryList>(list: &mut L, mut f: impl FnMut(&mut L::Entry) -> FilterStep) {
    let mut i = 0;
    while i < list.len() {
        let step = f(list.entry(i));
        if step.remove {
            list.remove(i);
        }
        if !step.keep_going {
            break;
        }
        if !step.remove {
            i += 1;
        }
    }
}
