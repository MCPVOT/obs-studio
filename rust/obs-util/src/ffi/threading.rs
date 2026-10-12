//! C ABI shim for `libobs/util/threading-posix.c` and
//! `libobs/util/threading-windows.c`.
//!
//! NULL arguments where the C would dereference the pointer (`os_event_*`
//! except destroy, the out-pointer of `os_event_init`/`os_sem_init`) get
//! `EINVAL` here — the C behavior is undefined and the parity test excludes
//! them. `os_sem_post`/`os_sem_wait` have a defined NULL check in the C
//! (`!sem` → -1), kept.

#[cfg(unix)]
pub use posix::*;

#[cfg(windows)]
pub use windows::*;

/// C ABI for `util/threading-windows.c`. NULL event handles are defined
/// in that file (`EINVAL`, or a no-op for destroy/reset). A NULL
/// out-pointer on init is undefined in the C once the kernel object
/// exists; the shim closes it and returns `EINVAL`.
#[cfg(windows)]
mod windows {
    use core::ffi::{c_char, c_int, c_ulong};

    use crate::platform::windows::{Event, Sem};

    /// `OS_EVENT_TYPE_MANUAL`.
    const OS_EVENT_TYPE_MANUAL: c_int = 1;

    /// # Safety
    /// `event` is a live out-pointer or null. `type_` is `enum os_event_type`;
    /// anything other than manual is auto, as in C.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_init(event: *mut *mut Event, type_: c_int) -> c_int {
        let Some(created) = Event::try_new(type_ == OS_EVENT_TYPE_MANUAL) else {
            return -1;
        };
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is checked non-null.
        unsafe { *event = Box::into_raw(Box::new(created)) };
        0
    }

    /// # Safety
    /// `event` came from [`os_event_init`] or is null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_destroy(event: *mut Event) {
        if event.is_null() {
            return;
        }
        // SAFETY: `event` is a live box from `os_event_init`, dropped once.
        unsafe { drop(Box::from_raw(event)) };
    }

    /// # Safety
    /// `event` came from [`os_event_init`] or is null (`EINVAL`).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_wait(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.wait_code()
    }

    /// # Safety
    /// Same contract as [`os_event_wait`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_timedwait(event: *mut Event, milliseconds: c_ulong) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.wait_for(milliseconds)
    }

    /// # Safety
    /// Same contract as [`os_event_wait`]. A timeout is `EAGAIN`, as in C.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_try(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        let code = unsafe { &*event }.wait_for(0);
        if code == libc::ETIMEDOUT {
            libc::EAGAIN
        } else {
            code
        }
    }

    /// # Safety
    /// Same contract as [`os_event_wait`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_signal(event: *mut Event) -> c_int {
        if event.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.signal_code()
    }

    /// # Safety
    /// `event` came from [`os_event_init`] or is null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_event_reset(event: *mut Event) {
        if event.is_null() {
            return;
        }
        // SAFETY: `event` is a live `Event` when non-null.
        unsafe { &*event }.reset();
    }

    /// # Safety
    /// `sem` is a live out-pointer or null. A negative `value` fails with
    /// -1 before the out-pointer is touched, as `CreateSemaphore` does.
    /// A null out-pointer after a successful create is undefined in C;
    /// the handle is closed and this returns `EINVAL`.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_init(sem: *mut *mut Sem, value: c_int) -> c_int {
        if value < 0 {
            return -1;
        }
        let Some(created) = Sem::try_new(value as u32) else {
            return -1;
        };
        if sem.is_null() {
            return libc::EINVAL;
        }
        // SAFETY: `sem` is checked non-null. `Sem` is `Unpin`.
        unsafe { *sem = Box::into_raw(std::pin::Pin::into_inner(created)) };
        0
    }

    /// # Safety
    /// `sem` came from [`os_sem_init`] or is null.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_destroy(sem: *mut Sem) {
        if sem.is_null() {
            return;
        }
        // SAFETY: `sem` is a live box from `os_sem_init`, dropped once.
        unsafe { drop(Box::from_raw(sem)) };
    }

    /// # Safety
    /// `sem` came from [`os_sem_init`] or is null (-1, as in C).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_post(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.post()
    }

    /// # Safety
    /// Same contract as [`os_sem_post`].
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_wait(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.wait()
    }

    /// # Safety
    /// `name` is a C string or null, as `threading-windows.c` accepts.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_set_thread_name(name: *const c_char) {
        // SAFETY: `name` is a C string or null.
        unsafe { crate::platform::windows::set_thread_name(name) };
    }
}

#[cfg(unix)]
mod posix {
    use core::ffi::{c_char, c_int, c_ulong, c_void};
    use core::ptr;

    use crate::ffi::darray::{bfree, bmalloc};
    use crate::threading::{Event, EventType, Sem};

    /// `OS_EVENT_TYPE_MANUAL` in `enum os_event_type`.
    const OS_EVENT_TYPE_MANUAL: c_int = 1;

    /// The C's `bzalloc`: `bmalloc` + zero-fill. Routing every handle
    /// through it keeps each live event/semaphore one `bnum_allocs` unit,
    /// exactly as `bzalloc(sizeof(struct os_*_data))` does in the C.
    /// `bmalloc` cannot fail — it `bcrash`es on OOM like the C.
    unsafe fn bzalloc(size: usize) -> *mut c_void {
        // SAFETY: `bmalloc` returns `size` writable bytes or crashes.
        let p = unsafe { bmalloc(size) };
        // SAFETY: `p` is writable for `size` bytes.
        unsafe { ptr::write_bytes(p.cast::<u8>(), 0, size) };
        p
    }

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
        let data = unsafe { bzalloc(size_of::<Event>()) };
        // SAFETY: `data` is `size_of::<Event>()` writable bytes and `event`
        // is checked non-null above.
        unsafe {
            ptr::write(
                data.cast::<Event>(),
                Event::new(if manual {
                    EventType::Manual
                } else {
                    EventType::Auto
                }),
            );
            *event = data.cast();
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
        // SAFETY: `event` is a live `Event` from `os_event_init`, dropped
        // once; the block came from `bzalloc`.
        unsafe {
            ptr::drop_in_place(event);
            bfree(event.cast());
        }
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
    ///
    /// The C initializes the native semaphore first and `bzalloc`s the
    /// handle second; here the order is reversed because `sem_t` cannot
    /// be relocated into the block after `sem_init`. The net effect is
    /// identical: a failed init frees the block again, and a successful
    /// one leaves exactly one `bnum_allocs` unit.
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_init(sem: *mut *mut Sem, value: c_int) -> c_int {
        if sem.is_null() {
            return libc::EINVAL;
        }
        let data = unsafe { bzalloc(size_of::<Sem>()) };
        // SAFETY: `data` is `size_of::<Sem>()` writable bytes.
        let code = unsafe { Sem::init_in(data.cast(), value) };
        if code != 0 {
            // SAFETY: `data` came from `bzalloc` and was never made live.
            unsafe { bfree(data) };
            return code;
        }
        // SAFETY: `sem` is checked non-null above.
        unsafe {
            *sem = data.cast();
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
        // SAFETY: `sem` is a live `Sem` from `os_sem_init`, dropped once;
        // the block came from `bzalloc`.
        unsafe {
            ptr::drop_in_place(sem);
            bfree(sem.cast());
        }
    }

    /// # Safety
    ///
    /// `sem` came from [`os_sem_init`] or is null (-1, as in C). Returns
    /// the primitive's own code like the C does — including -1 on
    /// EOVERFLOW past SEM_VALUE_MAX (non-Apple POSIX).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_post(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.post()
    }

    /// # Safety
    ///
    /// Same contract as [`os_sem_post`]. Returns the primitive's own code,
    /// including -1 when a signal interrupts the wait (EINTR).
    #[unsafe(no_mangle)]
    pub unsafe extern "C" fn os_sem_wait(sem: *mut Sem) -> c_int {
        if sem.is_null() {
            return -1;
        }
        // SAFETY: `sem` is a live `Sem` when non-null.
        unsafe { &*sem }.wait()
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
        #[cfg(target_os = "freebsd")]
        // SAFETY: `name` is a valid C string; names the calling thread.
        // The C names threads only on Apple, FreeBSD, and glibc — not
        // OpenBSD or DragonFly (threading-posix.c:258–274).
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
