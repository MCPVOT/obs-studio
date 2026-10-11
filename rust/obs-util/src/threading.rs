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
use core::ffi::c_int;
#[cfg(all(unix, not(target_os = "macos")))]
use core::ffi::c_uint;
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

/// Mach semaphore bindings for the `__APPLE__` half of
/// `threading-posix.c`; the libc crate does not carry mach/semaphore.h.
/// Signatures match `<mach/semaphore.h>`.
#[cfg(target_os = "macos")]
#[allow(non_camel_case_types)]
mod mach_sem {
    use core::ffi::{c_int, c_uint};

    pub type task_t = libc::mach_port_t;
    pub type semaphore_t = libc::mach_port_t;
    pub type sync_policy_t = c_int;

    pub const SYNC_POLICY_FIFO: sync_policy_t = 0;
    pub const KERN_SUCCESS: c_int = 0;

    /// `mach_timespec_t`.
    #[repr(C)]
    pub struct mach_timespec_t {
        pub tv_sec: c_uint,
        pub tv_nsec: c_int,
    }

    unsafe extern "C" {
        /// `mach_task_self()` — the exported `mach_port_t` global, read
        /// directly because the libc crate marks its wrapper deprecated.
        static mach_task_self_: task_t;

        pub fn semaphore_create(
            task: task_t,
            semaphore: *mut semaphore_t,
            policy: sync_policy_t,
            value: c_int,
        ) -> c_int;
        pub fn semaphore_destroy(task: task_t, semaphore: semaphore_t) -> c_int;
        pub fn semaphore_signal(semaphore: semaphore_t) -> c_int;
        pub fn semaphore_wait(semaphore: semaphore_t) -> c_int;
        pub fn semaphore_timedwait(semaphore: semaphore_t, timeout: mach_timespec_t) -> c_int;
    }

    /// `mach_task_self()`.
    pub fn mach_task_self() -> task_t {
        // SAFETY: read-only global.
        unsafe { mach_task_self_ }
    }
}

/// `os_sem_t`: the same kernel primitive the C uses per platform — a Mach
/// semaphore on macOS (`semaphore_create`/`semaphore_wait`/
/// `semaphore_signal`/`semaphore_destroy`), a POSIX `sem_t` elsewhere
/// (`sem_init`/`sem_wait`/`sem_post`/`sem_destroy`). Using the real
/// primitives is what preserves `os_sem_wait`'s -1 on EINTR and
/// `os_sem_post`'s -1 on EOVERFLOW past SEM_VALUE_MAX; neither is
/// observable through `std::sync::Condvar`.
///
/// The native object must not be moved once initialized, so a `Sem` is
/// always created in place: [`Sem::new`] returns it boxed and the FFI
/// layer writes it into a `bzalloc` block via [`Sem::init_in`].
#[cfg(unix)]
pub struct Sem {
    #[cfg(target_os = "macos")]
    task: mach_sem::task_t,
    #[cfg(target_os = "macos")]
    port: mach_sem::semaphore_t,
    #[cfg(not(target_os = "macos"))]
    sem: libc::sem_t,
}

#[cfg(unix)]
impl Sem {
    /// Initialize `*out` in place; returns the C entry point's code: 0 on
    /// success, -1 on failure. `sem_init` also sets errno (e.g. EINVAL on
    /// a negative value) on non-Apple POSIX; the macOS path maps
    /// `semaphore_create` failure to -1 with errno untouched, as the C
    /// does.
    ///
    /// # Safety
    /// `out` must point to `size_of::<Sem>()` writable bytes, treated as
    /// uninitialized. On success `*out` is a live `Sem` owning the native
    /// semaphore; release it with `ptr::drop_in_place`. A failed init
    /// leaves `*out` dead — nothing to drop.
    pub(crate) unsafe fn init_in(out: *mut Sem, value: c_int) -> c_int {
        #[cfg(target_os = "macos")]
        // SAFETY: `out` is writable for `*out`; only fields written here
        // are read later.
        unsafe {
            let task = mach_sem::mach_task_self();
            if mach_sem::semaphore_create(
                task,
                &raw mut (*out).port,
                mach_sem::SYNC_POLICY_FIFO,
                value,
            ) != mach_sem::KERN_SUCCESS
            {
                return -1;
            }
            (*out).task = task;
            0
        }
        #[cfg(not(target_os = "macos"))]
        // SAFETY: `out` is writable for `*out`.
        unsafe {
            libc::sem_init(&raw mut (*out).sem, 0, value as c_uint)
        }
    }

    /// `os_sem_init` for a nonnegative `value`. Panics if the OS rejects
    /// it — only resource exhaustion can fail here (a negative value is
    /// unrepresentable); the FFI failure channel is [`Sem::init_in`].
    pub fn new(value: u32) -> Box<Self> {
        let mut sem = Box::<Self>::new_uninit();
        // SAFETY: `sem` is `size_of::<Sem>()` writable uninit bytes.
        match unsafe { Self::init_in(sem.as_mut_ptr(), value as c_int) } {
            // SAFETY: `*sem` is a live `Sem`.
            0 => unsafe { sem.assume_init() },
            rc => panic!("os_sem_init({value}) failed: {rc}"),
        }
    }

    /// `os_sem_post`: 0 on success, -1 on failure (e.g. EOVERFLOW past
    /// SEM_VALUE_MAX on non-Apple POSIX).
    pub fn post(&self) -> c_int {
        #[cfg(target_os = "macos")]
        unsafe {
            if mach_sem::semaphore_signal(self.port) == mach_sem::KERN_SUCCESS {
                0
            } else {
                -1
            }
        }
        #[cfg(not(target_os = "macos"))]
        // SAFETY: `self.sem` was initialized at `init_in` and never moved.
        unsafe {
            libc::sem_post((&raw const self.sem).cast_mut())
        }
    }

    /// `os_sem_wait`: 0 on success, -1 on failure — including EINTR when a
    /// signal interrupts the wait, exactly as `sem_wait` reports it.
    pub fn wait(&self) -> c_int {
        #[cfg(target_os = "macos")]
        unsafe {
            if mach_sem::semaphore_wait(self.port) == mach_sem::KERN_SUCCESS {
                0
            } else {
                -1
            }
        }
        #[cfg(not(target_os = "macos"))]
        // SAFETY: `self.sem` was initialized at `init_in` and never moved.
        unsafe {
            libc::sem_wait((&raw const self.sem).cast_mut())
        }
    }

    /// Non-blocking take (`sem_trywait`; a zero-timeout
    /// `semaphore_timedwait` on macOS). Not part of `util/threading.h`;
    /// exists for the tests, which use it to observe the credit count.
    /// Returns true when a credit was consumed.
    pub fn try_wait(&self) -> bool {
        #[cfg(target_os = "macos")]
        unsafe {
            mach_sem::semaphore_timedwait(
                self.port,
                mach_sem::mach_timespec_t {
                    tv_sec: 0,
                    tv_nsec: 0,
                },
            ) == mach_sem::KERN_SUCCESS
        }
        #[cfg(not(target_os = "macos"))]
        // SAFETY: `self.sem` was initialized at `init_in` and never moved.
        unsafe {
            libc::sem_trywait((&raw const self.sem).cast_mut()) == 0
        }
    }
}

#[cfg(unix)]
impl Drop for Sem {
    fn drop(&mut self) {
        #[cfg(target_os = "macos")]
        // SAFETY: `self` is a live `Sem` from `init_in`.
        unsafe {
            mach_sem::semaphore_destroy(self.task, self.port);
        }
        #[cfg(not(target_os = "macos"))]
        // SAFETY: `self` is a live `Sem` from `init_in`, destroyed once.
        unsafe {
            libc::sem_destroy(&raw mut self.sem);
        }
    }
}

/// glibc's 15-byte thread-name limit: `os_set_thread_name` truncates the
/// name on `__GLIBC__` and passes it whole everywhere else.
pub fn gnu_thread_name(name: &[u8]) -> &[u8] {
    const MAX_GNU_THREAD_NAME: usize = 15;
    &name[..name.len().min(MAX_GNU_THREAD_NAME)]
}
