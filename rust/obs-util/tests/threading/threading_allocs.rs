//! `bnum_allocs` accounting for the threading handles: the C allocates
//! `struct os_event_data`/`struct os_sem_data` with `bzalloc`, so every
//! live handle is one allocation unit. In a `Box`-allocated port the
//! counter never moves.
//!
//! `bnum_allocs` is a process-global counter, so asserting absolute
//! deltas races with any concurrent allocation, including the other tests
//! of this binary. The measurement therefore runs alone in a child process
//! (`common::isolated`, #1133).
//!
//! Unix only, like the symbols it exercises.

#![cfg(unix)]

use std::ffi::c_void;
use std::ptr;
use std::sync::Arc;
use std::sync::atomic::{AtomicBool, Ordering};
use std::thread;

use obs_c_oracle as _; // links the oracle test stubs
use obs_util::ffi::bmem::{bfree, bmalloc, bnum_allocs};
use obs_util::ffi::threading::{os_event_destroy, os_event_init, os_sem_destroy, os_sem_init};
use obs_util::threading::{Event, Sem};

/// The test's measurement: absolute deltas of the process-wide counter, so
/// nothing else in the process may allocate through bmem while it runs.
fn measure() {
    let base = unsafe { bnum_allocs() };

    let mut event: *mut Event = ptr::null_mut();
    assert_eq!(unsafe { os_event_init(&mut event, 0) }, 0);
    assert_eq!(unsafe { bnum_allocs() }, base + 1);

    let mut sem: *mut Sem = ptr::null_mut();
    assert_eq!(unsafe { os_sem_init(&mut sem, 0) }, 0);
    assert_eq!(unsafe { bnum_allocs() }, base + 2);

    unsafe {
        os_event_destroy(event);
        os_sem_destroy(sem);
    }
    assert_eq!(unsafe { bnum_allocs() }, base);
}

const INIT_DESTROY: &str = concat!(
    module_path!(),
    "::init_destroy_counts_one_bzalloc_per_live_handle"
);

/// Measured in a child process of its own (#1133): the parent's other tests
/// allocate through bmem in parallel.
#[test]
fn init_destroy_counts_one_bzalloc_per_live_handle() {
    if crate::common::isolated(INIT_DESTROY) {
        measure();
    }
}

/// #1133: this binary runs ~40 tests in parallel and others allocate through
/// bmem, which moved the counter mid-measurement. Another thread allocating
/// the whole time must not break the check.
#[test]
fn holds_under_concurrent_allocation() {
    let stop = Arc::new(AtomicBool::new(false));
    let before = unsafe { bnum_allocs() };
    let allocator = {
        let stop = Arc::clone(&stop);
        thread::spawn(move || {
            let mut held = Vec::new();
            while !stop.load(Ordering::Relaxed) {
                held.push(unsafe { bmalloc(16) } as usize);
                thread::yield_now();
            }
            for p in held {
                unsafe { bfree(p as *mut c_void) };
            }
        })
    };
    // let the allocator get going
    while unsafe { bnum_allocs() } <= before {
        thread::yield_now();
    }

    // the check runs in its own process, which this thread does not touch
    let result = std::panic::catch_unwind(|| {
        assert!(!crate::common::isolated(INIT_DESTROY));
    });

    stop.store(true, Ordering::Relaxed);
    allocator.join().unwrap();
    if let Err(panic) = result {
        std::panic::resume_unwind(panic);
    }
}
