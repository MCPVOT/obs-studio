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

pub use crate::platform::Sem;
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

/// Which condvar wake primitive `signal` dispatches. C signals with
/// `pthread_cond_signal`, so `One` is correct; recording the dispatch
/// choice lets a unit test assert it exactly, which a wake count under
/// real concurrency cannot (a manual event legitimately releases an
/// extra waiter on a spurious wakeup).
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Notify {
    One = 1,
    /// Present so the seam can represent — and the test can detect — a
    /// `notify_all` dispatch even though correct code never sends one.
    #[allow(dead_code)]
    All = 2,
}

/// Condvar that records which wake primitive each call dispatches. The
/// record lives inside the forwarding method, so a mutant that changes
/// `One`'s action to `notify_all` is observed as `All` — a store beside
/// the call would not see the mutation.
struct NotifyCondvar(Condvar, #[cfg(test)] std::sync::atomic::AtomicU8);

impl NotifyCondvar {
    #[inline]
    fn new() -> Self {
        Self(
            Condvar::new(),
            #[cfg(test)]
            std::sync::atomic::AtomicU8::new(0),
        )
    }

    #[inline]
    fn notify_one(&self) {
        #[cfg(test)]
        self.1.store(1, std::sync::atomic::Ordering::SeqCst);
        self.0.notify_one();
    }

    #[inline]
    #[allow(dead_code)]
    fn notify_all(&self) {
        #[cfg(test)]
        self.1.store(2, std::sync::atomic::Ordering::SeqCst);
        self.0.notify_all();
    }

    #[inline]
    fn wait<'a>(&self, guard: MutexGuard<'a, bool>) -> std::sync::LockResult<MutexGuard<'a, bool>> {
        self.0.wait(guard)
    }

    #[inline]
    fn wait_timeout<'a>(
        &self,
        guard: MutexGuard<'a, bool>,
        dur: Duration,
    ) -> std::sync::LockResult<(MutexGuard<'a, bool>, std::sync::WaitTimeoutResult)> {
        self.0.wait_timeout(guard, dur)
    }
}

/// `os_event_t`: mutex + condvar + `signalled`/`manual` in the C.
pub struct Event {
    signalled: Mutex<bool>,
    cond: NotifyCondvar,
    manual: bool,
    /// Threads currently blocked inside a condvar wait call. Debug
    /// builds only; lets stress tests prove every waiter is asleep
    /// before signalling instead of relying on a settle window.
    #[cfg(debug_assertions)]
    waiting: std::sync::atomic::AtomicUsize,
}

impl Event {
    pub fn new(event_type: EventType) -> Self {
        Self {
            signalled: Mutex::new(false),
            cond: NotifyCondvar::new(),
            manual: event_type == EventType::Manual,
            #[cfg(debug_assertions)]
            waiting: std::sync::atomic::AtomicUsize::new(0),
        }
    }

    /// Single dispatch point for condvar wakes; `signal` picks `One`,
    /// matching `pthread_cond_signal`. The [`NotifyCondvar`] records
    /// the primitive actually invoked, inside the forwarding method.
    fn notify(&self, kind: Notify) {
        match kind {
            Notify::One => self.cond.notify_one(),
            Notify::All => self.cond.notify_all(),
        }
    }

    /// Wake primitive last dispatched through the condvar: `Some(One)`
    /// for `notify_one`, `Some(All)` for `notify_all`, `None` if none
    /// has run yet. Test builds only.
    #[cfg(test)]
    fn last_notify_kind(&self) -> Option<Notify> {
        match self.cond.1.load(std::sync::atomic::Ordering::SeqCst) {
            1 => Some(Notify::One),
            2 => Some(Notify::All),
            _ => None,
        }
    }

    /// Number of threads blocked inside a wait call right now. Debug
    /// builds only (the lib is built in dev profile for `cargo test`);
    /// lets a test gate a signal on every waiter being asleep rather
    /// than on a timing window.
    #[cfg(debug_assertions)]
    #[doc(hidden)]
    pub fn waiting_count(&self) -> usize {
        self.waiting.load(std::sync::atomic::Ordering::Acquire)
    }

    #[inline]
    fn begin_wait(&self) {
        #[cfg(debug_assertions)]
        self.waiting
            .fetch_add(1, std::sync::atomic::Ordering::AcqRel);
    }

    #[inline]
    fn end_wait(&self) {
        #[cfg(debug_assertions)]
        self.waiting
            .fetch_sub(1, std::sync::atomic::Ordering::AcqRel);
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
            self.begin_wait();
            signalled = self.cond.wait(signalled).unwrap_or_else(|e| e.into_inner());
            self.end_wait();
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
            self.begin_wait();
            let (guard, result) = self
                .cond
                .wait_timeout(signalled, timeout)
                .unwrap_or_else(|e| e.into_inner());
            self.end_wait();
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
        self.notify(Notify::One);
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

#[cfg(test)]
mod tests {
    use super::*;

    /// `os_event_signal` must dispatch `pthread_cond_signal`'s
    /// equivalent — one waiter woken. A `notify_all` mutation is
    /// observed exactly here: the seam records the dispatch decision,
    /// which does not depend on scheduling or waiters being asleep.
    #[test]
    fn signal_dispatches_notify_one() {
        for ty in [EventType::Auto, EventType::Manual] {
            let ev = Event::new(ty);
            ev.signal();
            assert_eq!(ev.last_notify_kind(), Some(Notify::One));
        }
    }
}
