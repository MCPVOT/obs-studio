//! Safe core for `libobs/util/threading-posix.c` (the POSIX half of
//! `util/threading.h`): auto/manual-reset events, a counting semaphore,
//! and the glibc thread-name truncation rule.
//!
//! The C ABI shim lives in [`crate::ffi::threading`]. The atomics in
//! `threading-posix.h` are header inlines compiled into every C caller;
//! they are not part of this unit.
//!
//! Documented difference (testing-policy known difference):
//! `Event::timed_wait` uses a relative monotonic timeout
//! (`Condvar::wait_timeout`), while the C recomputes a
//! `CLOCK_REALTIME`/`gettimeofday` absolute deadline on every loop
//! iteration. The two diverge only across a wall-clock jump during a
//! wait: the C can then expire early or late where the Rust completes
//! the full relative span. Spurious wakeups behave identically — both
//! restart the full timeout.

#[cfg(unix)]
pub use crate::ffi::native_sem::Sem;
use std::sync::{Condvar, Mutex, MutexGuard};
use std::time::Duration;

/// `enum os_event_type`.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EventType {
    /// `OS_EVENT_TYPE_AUTO`: a successful wait consumes the signal.
    Auto,
    /// `OS_EVENT_TYPE_MANUAL`: the event stays signalled until `reset`.
    Manual,
}

/// `os_event_t`: mutex + condvar + `signalled`/`manual` in the C.
pub struct Event {
    signalled: Mutex<bool>,
    cond: Condvar,
    manual: bool,
}

impl Event {
    pub fn new(event_type: EventType) -> Self {
        Self {
            signalled: Mutex::new(false),
            cond: Condvar::new(),
            manual: event_type == EventType::Manual,
        }
    }

    /// Consume the signal, as the C does on a successful wait/try of an
    /// auto-reset event. Caller holds the lock.
    fn consume(&self, signalled: &mut MutexGuard<'_, bool>) {
        if !self.manual {
            **signalled = false;
        }
    }

    /// `os_event_wait`: block until signalled; an auto event consumes it.
    /// Poison is treated as success: the C mutex has no poisoning, and a
    /// panicking neighbor thread must not break the primitive.
    pub fn wait(&self) {
        let mut signalled = self.signalled.lock().unwrap_or_else(|e| e.into_inner());
        while !*signalled {
            signalled = self.cond.wait(signalled).unwrap_or_else(|e| e.into_inner());
        }
        self.consume(&mut signalled);
    }

    /// `os_event_timedwait`: like `wait` but gives up after `milliseconds`;
    /// returns whether the event was signalled. The C recomputes the
    /// absolute deadline from the current time on every loop iteration, so
    /// a spurious wakeup restarts the full timeout — mirrored here.
    /// `milliseconds` is the C `unsigned long` (`u64` on LP64, which is
    /// every Unix this shim compiles for).
    pub fn timed_wait(&self, milliseconds: u64) -> bool {
        let timeout = Duration::from_millis(milliseconds);
        let mut signalled = self.signalled.lock().unwrap_or_else(|e| e.into_inner());
        while !*signalled {
            let (guard, result) = self
                .cond
                .wait_timeout(signalled, timeout)
                .unwrap_or_else(|e| e.into_inner());
            signalled = guard;
            if result.timed_out() && !*signalled {
                return false;
            }
        }
        self.consume(&mut signalled);
        true
    }

    /// `os_event_try`: non-blocking wait; an auto event consumes the signal.
    pub fn try_wait(&self) -> bool {
        let mut signalled = self.signalled.lock().unwrap_or_else(|e| e.into_inner());
        if !*signalled {
            return false;
        }
        self.consume(&mut signalled);
        true
    }

    /// `os_event_signal`.
    pub fn signal(&self) {
        *self.signalled.lock().unwrap_or_else(|e| e.into_inner()) = true;
        self.cond.notify_one();
    }

    /// `os_event_reset`.
    pub fn reset(&self) {
        *self.signalled.lock().unwrap_or_else(|e| e.into_inner()) = false;
    }
}

/// glibc's 15-byte thread-name limit: `os_set_thread_name` truncates the
/// name on `__GLIBC__` and passes it whole everywhere else.
pub fn gnu_thread_name(name: &[u8]) -> &[u8] {
    const MAX_GNU_THREAD_NAME: usize = 15;
    &name[..name.len().min(MAX_GNU_THREAD_NAME)]
}
