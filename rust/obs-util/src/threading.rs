//! Safe core for `libobs/util/threading-posix.c` (the POSIX half of
//! `util/threading.h`): auto/manual-reset events, a counting semaphore,
//! and the glibc thread-name truncation rule.
//!
//! The C ABI shim lives in [`crate::ffi::threading`]. The atomics in
//! `threading-posix.h` are header inlines compiled into every C caller;
//! they are not part of this unit.

use std::sync::{Condvar, Mutex};

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

    /// `os_event_wait`: block until signalled; an auto event consumes it.
    pub fn wait(&self) {
        todo!()
    }

    /// `os_event_timedwait`: like `wait` but gives up after `milliseconds`;
    /// returns whether the event was signalled.
    pub fn timed_wait(&self, _milliseconds: u64) -> bool {
        todo!()
    }

    /// `os_event_try`: non-blocking wait; an auto event consumes the signal.
    pub fn try_wait(&self) -> bool {
        todo!()
    }

    /// `os_event_signal`.
    pub fn signal(&self) {
        todo!()
    }

    /// `os_event_reset`.
    pub fn reset(&self) {
        todo!()
    }
}

/// `os_sem_t`: a counting semaphore.
pub struct Sem {
    count: Mutex<usize>,
    avail: Condvar,
}

impl Sem {
    pub fn new(value: u32) -> Self {
        let _ = value;
        Self {
            count: Mutex::new(0),
            avail: Condvar::new(),
        }
    }

    /// `os_sem_post`.
    pub fn post(&self) {
        todo!()
    }

    /// `os_sem_wait`: block until a count is available, then take one.
    pub fn wait(&self) {
        todo!()
    }
}

/// glibc's 15-byte thread-name limit: `os_set_thread_name` truncates the
/// name on `__GLIBC__` and passes it whole everywhere else.
pub fn gnu_thread_name(name: &[u8]) -> &[u8] {
    let _ = name;
    todo!()
}
