//! `Dstr::replace` does no heap work when nothing matches (#1296).
//!
//! Counting allocations on the calling thread is deterministic, unlike a
//! wall-clock bound: the old replace built a full copy of the string before
//! learning there was no match, which is what made `dstr/safe_printf` 6.6x
//! to 58x slower than C (one replace per `$n` placeholder).

use std::alloc::{GlobalAlloc, Layout, System};
use std::cell::Cell;

use obs_util::dstr::Dstr;

struct Counting;

thread_local! {
    static ALLOCS: Cell<usize> = const { Cell::new(0) };
}

// SAFETY: forwards every call to the system allocator unchanged.
unsafe impl GlobalAlloc for Counting {
    unsafe fn alloc(&self, layout: Layout) -> *mut u8 {
        let _ = ALLOCS.try_with(|n| n.set(n.get() + 1));
        // SAFETY: same contract as the caller's.
        unsafe { System.alloc(layout) }
    }
    unsafe fn dealloc(&self, ptr: *mut u8, layout: Layout) {
        // SAFETY: same contract as the caller's.
        unsafe { System.dealloc(ptr, layout) }
    }
    unsafe fn realloc(&self, ptr: *mut u8, layout: Layout, new_size: usize) -> *mut u8 {
        let _ = ALLOCS.try_with(|n| n.set(n.get() + 1));
        // SAFETY: same contract as the caller's.
        unsafe { System.realloc(ptr, layout, new_size) }
    }
}

#[global_allocator]
static GLOBAL: Counting = Counting;

fn allocs_during(f: impl FnOnce()) -> usize {
    let before = ALLOCS.with(Cell::get);
    f();
    ALLOCS.with(Cell::get) - before
}

#[test]
fn replace_without_match_does_not_allocate() {
    let mut d = Dstr::new();
    d.copy("obs ".repeat(1024).as_bytes());
    let before = d.as_bytes().to_vec();
    assert_eq!(allocs_during(|| d.replace(b"$1", b"value")), 0);
    assert_eq!(d.as_bytes(), &before[..]);
}

#[test]
fn safe_printf_unused_placeholders_only_copy_the_format() {
    let mut d = Dstr::new();
    let fmt = b"no placeholders here".to_vec();
    // One allocation for the format copy; the `$1`/`$2` scans add none.
    assert_eq!(
        allocs_during(|| d.safe_printf(Some(&fmt), [Some(b"x"), Some(b"y"), None, None])),
        1
    );
}

#[test]
fn replace_with_matches_allocates_at_most_once() {
    let mut d = Dstr::new();
    d.copy("obs ".repeat(1024).as_bytes());
    assert!(allocs_during(|| d.replace(b"obs", b"Rust OBS")) <= 1);
    assert_eq!(d.as_bytes(), "Rust OBS ".repeat(1024).as_bytes());
}
