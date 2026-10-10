//! C ABI shim for `libobs/util/threading-posix.c`.
//!
//! The whole surface is Unix-only (`#[cfg(unix)]`): on Windows the same
//! symbols come from `util/threading-windows.c`, which stays in C and is
//! not replaced by this port.
//!
//! NULL arguments where the C would dereference the pointer (`os_event_*`
//! except destroy, the out-pointer of `os_event_init`/`os_sem_init`) get
//! `EINVAL` here — the C behavior is undefined and the parity test excludes
//! them. `os_sem_post`/`os_sem_wait` have a defined NULL check in the C
//! (`!sem` → -1), kept.

#[cfg(unix)]
pub use posix::*;

#[cfg(unix)]
mod posix {
    use core::ffi::{c_char, c_int, c_ulong};

    use crate::threading::{Event, EventType, Sem};

    /// `OS_EVENT_TYPE_MANUAL` in `enum os_event_type`.
    const OS_EVENT_TYPE_MANUAL: c_int = 1;

    /// # Safety
    ///
    /// `event` is a live out-pointer, as in C; a null one returns EINVAL
    /// where C would crash. `type_` is `enum os_event_type`; anything other
    /// than `OS_EVENT_TYPE_MANUAL` means auto, as in C.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_init(event: *mut *mut Event, type_: c_int) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        let manual = type_ == OS_EVENT_TYPE_MANUAL;
        // SAFETY: `event` is checked non-null above.
        unsafe {
            *event = Box::into_raw(Box::new(Event::new(if manual {
                EventType::Manual
            } else {
                EventType::Auto
            })));
        }
        0
    }

    /// # Safety
    ///
    /// `event` came from [`os_event_init`] or is null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_destroy(event: *mut Event) {
        if event.is_null() {
            return;
        }
        // SAFETY: `event` is a live `Box<Event>` we allocated, destroyed once.
        unsafe { drop(Box::from_raw(event)) };
    }

    /// # Safety
    ///
    /// `event` came from [`os_event_init`]. A null one returns EINVAL where
    /// C would crash (listed in `tests/threading_parity.rs`).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_wait(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.wait();
        0
    }

    /// # Safety
    ///
    /// Same contract as [`os_event_wait`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_timedwait(event: *mut Event, milliseconds: c_ulong) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        if unsafe { &*event }.timed_wait(milliseconds) {
            0
        } else {
            libc::ETIMEDOUT
        }
    }

    /// # Safety
    ///
    /// Same contract as [`os_event_wait`]; a null event returns EAGAIN, the
    /// "not signalled" code.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_try(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EAGAIN;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        if unsafe { &*event }.try_wait() {
            0
        } else {
            libc::EAGAIN
        }
    }

    /// # Safety
    ///
    /// Same contract as [`os_event_wait`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_signal(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.signal();
        0
    }

    /// # Safety
    ///
    /// `event` came from [`os_event_init`] or is null (C would crash on
    /// null; ignored here, as `os_event_destroy` treats it).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_reset(event: *mut Event) {
        if event.is_null() {
            return;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.reset();
    }

    /// # Safety
    ///
    /// `sem` is a live out-pointer, as in C; a null one returns EINVAL.
    /// A negative `value` fails the way the C does on every Unix: -1,
    /// either from `semaphore_create` (macOS) or from `sem_init`, which
    /// returns -1 itself while EINVAL only appears in errno.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_init(sem: *mut *mut Sem, value: c_int) -> c_int {
        if sem.is_null() {
            return libc::EINVAL;
        }
        if value < 0 {
            return -1;
        }
        // SAFETY: `sem` is checked non-null above.
        unsafe {
            *sem = Box::into_raw(Box::new(Sem::new(value as u32)));
        }
        0
    }

    /// # Safety
    ///
    /// `sem` came from [`os_sem_init`] or is null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_destroy(sem: *mut Sem) {
        if sem.is_null() {
            return;
        }
        // SAFETY: `sem` is a live `Box<Sem>` we allocated, destroyed once.
        unsafe { drop(Box::from_raw(sem)) };
    }

    /// # Safety
    ///
    /// `sem` came from [`os_sem_init`] or is null (-1, as in C).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_post(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.post();
        0
    }

    /// # Safety
    ///
    /// Same contract as [`os_sem_post`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_wait(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.wait();
        0
    }

    /// # Safety
    ///
    /// `name` is a C string or null. Null is ignored; C would crash in
    /// `strlen`/`pthread_setname_np` (listed in `tests/threading_parity.rs`).
    /// Platform branches mirror `threading-posix.c`: glibc truncates to 15
    /// bytes, macOS takes the name whole, FreeBSD names the calling thread,
    /// anything else (e.g. musl, where `__GLIBC__` is also undefined) is a
    /// no-op.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_set_thread_name(name: *const c_char) {
        if name.is_null() {
            return;
        }
        #[cfg(target_os = "macos")]
        // SAFETY: `name` is a valid C string.
        unsafe {
            libc::pthread_setname_np(name);
        }
        #[cfg(any(target_os = "freebsd", target_os = "dragonfly", target_os = "openbsd"))]
        // SAFETY: `name` is a valid C string; names the calling thread.
        unsafe {
            libc::pthread_set_name_np(libc::pthread_self(), name);
        }
        #[cfg(all(target_os = "linux", target_env = "gnu"))]
        {
            let bytes = crate::threading::gnu_thread_name(
                // SAFETY: `name` is a valid C string.
                unsafe { core::ffi::CStr::from_ptr(name) }.to_bytes(),
            );
            let mut buf = [0u8; 16];
            buf[..bytes.len()].copy_from_slice(bytes);
            // SAFETY: `buf` is a NUL-terminated name of at most 15 bytes.
            unsafe {
                libc::pthread_setname_np(libc::pthread_self(), buf.as_ptr().cast());
            }
        }
        #[cfg(not(any(
            target_os = "macos",
            target_os = "freebsd",
            target_os = "dragonfly",
            target_os = "openbsd",
            all(target_os = "linux", target_env = "gnu")
        )))]
        let _ = name;
    }
}
