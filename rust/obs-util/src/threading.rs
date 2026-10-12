//! Safe core for `util/threading.h`: auto/manual-reset events, a counting
//! semaphore, and the glibc thread-name truncation rule.
//!
//! The event and the semaphore are the platform implementations in
//! [`crate::platform`]. Unix keeps the pthread condvar from
//! `threading-posix.c` and the native semaphore (`sem_t` or a Mach port).
//! Windows keeps the Win32 event and semaphore from `threading-windows.c`.
//!
//! The C ABI shim lives in [`crate::ffi::threading`]. The atomics in
//! `threading-posix.h` and `threading-windows.h` are header inlines compiled
//! into every C caller; they are not part of this unit.
//!
//! Documented difference (testing-policy known difference), Unix events only:
//! `Event::timed_wait` uses a relative monotonic timeout
//! (`Condvar::wait_timeout`), while the C recomputes a
//! `CLOCK_REALTIME`/`gettimeofday` absolute deadline on every loop
//! iteration. The two diverge only across a wall-clock jump during a
//! wait. Spurious wakeups behave the same: both restart the full timeout.

pub use crate::platform::{Event, EventType, Sem};

/// glibc's 15-byte thread-name limit: `os_set_thread_name` truncates the
/// name on `__GLIBC__` and passes it whole everywhere else.
pub fn gnu_thread_name(name: &[u8]) -> &[u8] {
    const MAX_GNU_THREAD_NAME: usize = 15;
    &name[..name.len().min(MAX_GNU_THREAD_NAME)]
}
