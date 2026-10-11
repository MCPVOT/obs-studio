//! Native semaphore backing `os_sem_t` — Unix only (Windows semaphores
//! come from `util/threading-windows.c`, which stays in C).
//!
//! The type is re-exported as `crate::threading::Sem`, the safe API the
//! Tier 1 tests exercise; it lives here, not in `src/threading.rs`,
//! because the testing policy allows `unsafe` only under `src/ffi/`.
//!
//! `os_sem_t` is the same kernel primitive the C uses per platform — a
//! Mach semaphore on macOS (`semaphore_create`/`semaphore_wait`/
//! `semaphore_signal`/`semaphore_destroy`), a POSIX `sem_t` elsewhere
//! (`sem_init`/`sem_wait`/`sem_post`/`sem_destroy`). Using the real
//! primitives is what preserves `os_sem_wait`'s -1 on EINTR and
//! `os_sem_post`'s -1 on EOVERFLOW past SEM_VALUE_MAX; neither is
//! observable through `std::sync::Condvar`.
//!
//! The native object must not be moved once initialized, and the type
//! system enforces it: a `sem_t` is mutated through `&self` only inside
//! an [`UnsafeCell`], [`PhantomPinned`] makes `Sem` `!Unpin`, and the
//! only safe constructor returns [`Pin<Box<Sem>>`], so safe code cannot
//! move the initialized pointee out of its box. The FFI entry points
//! write a `Sem` into a `bzalloc` block with [`Sem::init_in`] and keep
//! it at that address until `os_sem_destroy`.

#[cfg(not(target_os = "macos"))]
use core::cell::UnsafeCell;
#[cfg(target_os = "macos")]
use core::ffi::c_int;
#[cfg(not(target_os = "macos"))]
use core::ffi::{c_int, c_uint};
use core::marker::PhantomPinned;
use core::pin::Pin;
use core::ptr;

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

/// `os_sem_t`. The native object is created in place and never moves;
/// see the module docs for how that is enforced. A `Sem` cannot be
/// moved out of its pinned box:
///
/// ```compile_fail
/// let sem = obs_util::threading::Sem::new(0);
/// let _moved = *sem; // cannot move out of a pinned Sem
/// ```
#[cfg(unix)]
pub struct Sem {
    #[cfg(target_os = "macos")]
    task: mach_sem::task_t,
    #[cfg(target_os = "macos")]
    port: mach_sem::semaphore_t,
    /// The `sem_t` is mutated by `sem_post`/`sem_wait`/`sem_trywait`
    /// through a shared `&Sem`, so it sits in an `UnsafeCell`; its
    /// address is taken with `raw_get`/`get`, never via a `&mut` alias.
    #[cfg(not(target_os = "macos"))]
    sem: UnsafeCell<libc::sem_t>,
    /// Keeps `Sem` `!Unpin` so `Pin<Box<Sem>>` cannot be unpinned into
    /// a movable `Box<Sem>`; written by [`Sem::init_in`], never read.
    _pin: PhantomPinned,
}

// SAFETY: `sem_post`/`sem_wait`/`sem_trywait`/`sem_destroy` and the Mach
// `semaphore_*` calls are documented thread-safe on a shared semaphore;
// the `UnsafeCell` inside is only ever handed to them by pointer.
#[cfg(unix)]
unsafe impl Sync for Sem {}

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
        // SAFETY: `out` is writable for `*out`; `_pin` is a ZST written
        // so the field is never read uninit.
        unsafe { ptr::addr_of_mut!((*out)._pin).write(PhantomPinned) };
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
        // SAFETY: `out` is writable for `*out`; `raw_get` forms the
        // `*mut sem_t` straight from the field's raw address, so no
        // reference to the uninit `sem_t` is ever created.
        unsafe {
            libc::sem_init(
                UnsafeCell::raw_get(ptr::addr_of_mut!((*out).sem)),
                0,
                value as c_uint,
            )
        }
    }

    /// `os_sem_init` for a nonnegative `value`. Panics if the OS rejects
    /// it — only resource exhaustion can fail here (a negative value is
    /// unrepresentable); the FFI failure channel is [`Sem::init_in`].
    /// Returns the `Sem` pinned in its box so the initialized native
    /// object cannot be moved.
    pub fn new(value: u32) -> Pin<Box<Self>> {
        let mut sem = Box::<Self>::new_uninit();
        // SAFETY: `sem` is `size_of::<Sem>()` writable uninit bytes.
        match unsafe { Self::init_in(sem.as_mut_ptr(), value as c_int) } {
            // SAFETY: `*sem` is a live `Sem`, boxed so it cannot move;
            // `Sem` is `!Unpin`.
            0 => unsafe { Pin::new_unchecked(sem.assume_init()) },
            rc => panic!("os_sem_init({value}) failed: {rc}"),
        }
    }

    /// Address of the native storage, so the tests can check it does
    /// not move across operations.
    #[doc(hidden)]
    pub fn native_addr(&self) -> usize {
        #[cfg(target_os = "macos")]
        {
            ptr::addr_of!(self.port) as usize
        }
        #[cfg(not(target_os = "macos"))]
        {
            self.sem.get() as usize
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
            libc::sem_post(self.sem.get())
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
            libc::sem_wait(self.sem.get())
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
            libc::sem_trywait(self.sem.get()) == 0
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
            libc::sem_destroy(self.sem.get());
        }
    }
}
