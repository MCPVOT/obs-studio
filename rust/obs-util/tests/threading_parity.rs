//! Tier 3: the event/semaphore/thread-name C ABI against the original C,
//! compiled as an oracle. Unix only: on Windows these symbols come from
//! `util/threading-windows.c`, which this port does not replace.
//!
//! `os_event_t` and `os_sem_t` are opaque in `util/threading.h`, so this
//! port adds no `#[repr(C)]` struct and no layout test.
//!
//! What is compared: the return code of every call in a random op
//! sequence run single-threaded, so nothing can block. `wait` and
//! `sem_wait` are only issued when a shadow model says they return
//! immediately; `timedwait` uses 0/1 ms so an unsignalled wait
//! deterministically reports ETIMEDOUT.
//!
//! Excluded from comparison, not generated here:
//! - NULL event handles and a NULL out-pointer: the C dereferences them
//!   (UB); the shim returns EINVAL/EAGAIN as documented in ffi/threading.rs.
//! - `os_sem_destroy`/`os_event_destroy(NULL)` and `os_sem_post`/`os_sem_wait`
//!   NULL: defined in the C (-1 for the sem pair); pinned in
//!   `null_args_match` instead of generated.
//! - The `volatile bool signalled` read inside `pthread_cond_wait`: not
//!   observable through the API.
//! - `timedwait` with timeouts long enough to race scheduling.
//!
//! Reminder: the mutation check (break the core, see this file fail) is done
//! once per port, not on every change.

#![cfg(unix)]

use core::ffi::{c_int, c_ulong};
use std::ffi::CString;
use std::ptr;

use obs_c_oracle::threading::{
    OracleEvent, OracleSem, oracle_os_event_destroy, oracle_os_event_init, oracle_os_event_reset,
    oracle_os_event_signal, oracle_os_event_timedwait, oracle_os_event_try, oracle_os_event_wait,
    oracle_os_sem_destroy, oracle_os_sem_init, oracle_os_sem_post, oracle_os_sem_wait,
    oracle_os_set_thread_name,
};
use obs_util::ffi::threading::{
    os_event_destroy, os_event_init, os_event_reset, os_event_signal, os_event_timedwait,
    os_event_try, os_event_wait, os_sem_destroy, os_sem_init, os_sem_post, os_sem_wait,
    os_set_thread_name,
};
use obs_util::threading::Sem;
use proptest::prelude::*;

const MANUAL: c_int = 1;

/// One deterministic operation on an event or a semaphore.
#[derive(Debug, Clone)]
enum Op {
    /// `os_event_signal`
    Signal,
    /// `os_event_try` — consumes an auto signal.
    Try,
    /// `os_event_reset`
    Reset,
    /// `os_event_timedwait` with a 0 or 1 ms timeout.
    TimedWait(u64),
    /// `os_event_wait`, only issued while the model says it is signalled.
    WaitIfSignalled,
    /// `os_sem_post`
    Post,
    /// `os_sem_wait`, only issued while the model count is positive.
    WaitIfPositive,
}

/// The C-callable event surface, once per implementation.
struct EventApi {
    init: unsafe fn(*mut *mut u8, c_int) -> c_int,
    destroy: unsafe fn(*mut u8),
    wait: unsafe fn(*mut u8) -> c_int,
    timedwait: unsafe fn(*mut u8, c_ulong) -> c_int,
    try_: unsafe fn(*mut u8) -> c_int,
    signal: unsafe fn(*mut u8) -> c_int,
    reset: unsafe fn(*mut u8),
}

/// The C-callable semaphore surface.
struct SemApi {
    init: unsafe fn(*mut *mut u8, c_int) -> c_int,
    destroy: unsafe fn(*mut u8),
    post: unsafe fn(*mut u8) -> c_int,
    wait: unsafe fn(*mut u8) -> c_int,
}

fn rust_event_api() -> EventApi {
    unsafe fn init(e: *mut *mut u8, t: c_int) -> c_int {
        unsafe { os_event_init(e.cast(), t) }
    }
    EventApi {
        init,
        destroy: |e| unsafe { os_event_destroy(e.cast()) },
        wait: |e| unsafe { os_event_wait(e.cast()) },
        timedwait: |e, ms| unsafe { os_event_timedwait(e.cast(), ms) },
        try_: |e| unsafe { os_event_try(e.cast()) },
        signal: |e| unsafe { os_event_signal(e.cast()) },
        reset: |e| unsafe { os_event_reset(e.cast()) },
    }
}

fn oracle_event_api() -> EventApi {
    unsafe fn init(e: *mut *mut u8, t: c_int) -> c_int {
        unsafe { oracle_os_event_init(e.cast::<*mut OracleEvent>(), t) }
    }
    EventApi {
        init,
        destroy: |e| unsafe { oracle_os_event_destroy(e.cast()) },
        wait: |e| unsafe { oracle_os_event_wait(e.cast()) },
        timedwait: |e, ms| unsafe { oracle_os_event_timedwait(e.cast(), ms) },
        try_: |e| unsafe { oracle_os_event_try(e.cast()) },
        signal: |e| unsafe { oracle_os_event_signal(e.cast()) },
        reset: |e| unsafe { oracle_os_event_reset(e.cast()) },
    }
}

fn rust_sem_api() -> SemApi {
    unsafe fn init(s: *mut *mut u8, v: c_int) -> c_int {
        unsafe { os_sem_init(s.cast(), v) }
    }
    SemApi {
        init,
        destroy: |s| unsafe { os_sem_destroy(s.cast()) },
        post: |s| unsafe { os_sem_post(s.cast()) },
        wait: |s| unsafe { os_sem_wait(s.cast()) },
    }
}

fn oracle_sem_api() -> SemApi {
    unsafe fn init(s: *mut *mut u8, v: c_int) -> c_int {
        unsafe { oracle_os_sem_init(s.cast::<*mut OracleSem>(), v) }
    }
    SemApi {
        init,
        destroy: |s| unsafe { oracle_os_sem_destroy(s.cast()) },
        post: |s| unsafe { oracle_os_sem_post(s.cast()) },
        wait: |s| unsafe { oracle_os_sem_wait(s.cast()) },
    }
}

/// Run the op sequence against one implementation; return the return codes.
fn run(manual: bool, ops: &[Op], api: &EventApi, sem_api: &SemApi) -> Vec<c_int> {
    let mut results = Vec::new();
    let mut event: *mut u8 = ptr::null_mut();
    let mut sem: *mut u8 = ptr::null_mut();
    let mut signalled = false;
    let mut count: i64 = 0;

    results.push(unsafe { (api.init)(&mut event, if manual { MANUAL } else { 0 }) });
    results.push(unsafe { (sem_api.init)(&mut sem, 0) });
    assert!(!event.is_null() && !sem.is_null());

    for op in ops {
        let code = match *op {
            Op::Signal => {
                signalled = true;
                unsafe { (api.signal)(event) }
            }
            Op::Try => {
                let c = unsafe { (api.try_)(event) };
                if c == 0 {
                    signalled = manual;
                }
                c
            }
            Op::Reset => {
                signalled = false;
                unsafe {
                    (api.reset)(event);
                }
                continue;
            }
            Op::TimedWait(ms) => {
                let c = unsafe { (api.timedwait)(event, ms as c_ulong) };
                if c == 0 {
                    signalled = manual;
                }
                c
            }
            Op::WaitIfSignalled => {
                if !signalled {
                    continue;
                }
                let c = unsafe { (api.wait)(event) };
                signalled = manual;
                c
            }
            Op::Post => {
                count += 1;
                unsafe { (sem_api.post)(sem) }
            }
            Op::WaitIfPositive => {
                if count <= 0 {
                    continue;
                }
                let c = unsafe { (sem_api.wait)(sem) };
                count -= 1;
                c
            }
        };
        results.push(code);
    }

    unsafe {
        (api.destroy)(event);
        (sem_api.destroy)(sem);
    }
    results
}

/// NULL-argument behavior the C defines or the shim chooses: identical on
/// both sides where the C defines it.
#[test]
fn null_args_match() {
    unsafe {
        // Defined in the C: sem post/wait return -1 on NULL.
        assert_eq!(os_sem_post(ptr::null_mut()), -1);
        assert_eq!(oracle_os_sem_post(ptr::null_mut()), -1);
        assert_eq!(os_sem_wait(ptr::null_mut()), -1);
        assert_eq!(oracle_os_sem_wait(ptr::null_mut()), -1);
        // Defined in the C: destroying NULL is a no-op.
        os_sem_destroy(ptr::null_mut());
        oracle_os_sem_destroy(ptr::null_mut());
        os_event_destroy(ptr::null_mut());
        oracle_os_event_destroy(ptr::null_mut());
    }
}

/// `os_sem_init` with a negative value fails the same per-OS way.
#[test]
fn sem_init_negative_matches() {
    for value in [-1, i32::MIN] {
        let mut rust_sem: *mut Sem = ptr::null_mut();
        let mut oracle_sem: *mut OracleSem = ptr::null_mut();
        let rust = unsafe { os_sem_init(&mut rust_sem, value) };
        let oracle = unsafe { oracle_os_sem_init(&mut oracle_sem, value) };
        assert_eq!(rust, oracle, "sem_init({value})");
        // Both sides must return -1 itself on every Unix: C returns
        // `sem_init`'s return value (errno EINVAL is separate), and
        // macOS `semaphore_create` failure is mapped to -1.
        assert_eq!(rust, -1, "sem_init({value}) return code");
        assert_eq!(rust_sem.is_null(), oracle_sem.is_null());
        unsafe {
            os_sem_destroy(rust_sem);
            oracle_os_sem_destroy(oracle_sem);
        }
    }
}

/// `os_set_thread_name` sets the same kernel-visible name in both
/// implementations, including past the glibc 15-byte limit.
#[cfg(any(target_os = "linux", target_os = "macos"))]
#[test]
fn thread_name_matches() {
    use std::ffi::CStr;

    fn current_thread_name() -> String {
        let mut buf = [0u8; 64];
        // SAFETY: `buf` is a valid (buffer, size) pair; pthread_getname_np
        // writes the calling thread's name on Linux and macOS.
        assert_eq!(
            unsafe {
                libc::pthread_getname_np(libc::pthread_self(), buf.as_mut_ptr().cast(), buf.len())
            },
            0
        );
        // SAFETY: the buffer is NUL-terminated on success.
        unsafe { CStr::from_ptr(buf.as_ptr().cast()) }
            .to_string_lossy()
            .into_owned()
    }

    for name in [
        "t",
        "test-thread",
        "exactly-15-bytes",
        "a-very-long-thread-name-over-limit",
        "name with spaces",
        "",
    ] {
        let c_name = CString::new(name).unwrap();
        unsafe { os_set_thread_name(c_name.as_ptr()) };
        let rust = current_thread_name();
        unsafe { oracle_os_set_thread_name(c_name.as_ptr()) };
        let oracle = current_thread_name();
        assert_eq!(rust, oracle, "thread name for {name:?}");
    }
}

proptest! {
    #![proptest_config(ProptestConfig::with_cases(64))]

    /// Random op sequences produce identical return codes.
    #[test]
    fn parity(
        manual in any::<bool>(),
        ops in prop::collection::vec(
            prop_oneof![
                3 => Just(Op::Signal),
                3 => Just(Op::Try),
                1 => Just(Op::Reset),
                2 => prop::sample::select(vec![0u64, 1]).prop_map(Op::TimedWait),
                2 => Just(Op::WaitIfSignalled),
                3 => Just(Op::Post),
                2 => Just(Op::WaitIfPositive),
            ],
            0..64,
        )
    ) {
        let rust = run(manual, &ops, &rust_event_api(), &rust_sem_api());
        let oracle = run(manual, &ops, &oracle_event_api(), &oracle_sem_api());
        assert_eq!(rust, oracle);
    }
}
