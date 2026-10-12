//! Win32 events and semaphores for `libobs/util/threading-windows.c`.
//!
//! The opaque pointer the shim hands out is a heap `Event` or `Sem`
//! owning the kernel handle. The C stores the `HANDLE` itself and does
//! not go through `bmalloc`; this uses `Box` for the same reason, so
//! `bnum_allocs` does not move. `CreateSemaphore`'s maximum count is
//! `0x7FFFFFFF`, and a post at that ceiling returns -1.

use core::ffi::{c_char, c_int, c_void};
use core::pin::Pin;
use core::ptr;
use core::sync::atomic::{AtomicUsize, Ordering};

use crate::platform::EventType;

type Handle = *mut c_void;
type Bool = i32;

const INFINITE: u32 = 0xffff_ffff;
const WAIT_OBJECT_0: u32 = 0;
const WAIT_TIMEOUT: u32 = 0x0000_0102;
/// `CreateSemaphore` maximum count in `threading-windows.c`.
const SEM_MAX: i32 = 0x7fff_ffff;

#[link(name = "kernel32")]
#[allow(non_snake_case)]
unsafe extern "system" {
    fn CloseHandle(object: Handle) -> Bool;
    fn CreateEventW(
        attributes: *const c_void,
        manual_reset: Bool,
        initial_state: Bool,
        name: *const u16,
    ) -> Handle;
    fn SetEvent(event: Handle) -> Bool;
    fn ResetEvent(event: Handle) -> Bool;
    fn WaitForSingleObject(object: Handle, milliseconds: u32) -> u32;
    fn CreateSemaphoreW(
        attributes: *const c_void,
        initial: i32,
        maximum: i32,
        name: *const u16,
    ) -> Handle;
    fn ReleaseSemaphore(semaphore: Handle, release: i32, previous: *mut i32) -> Bool;
    fn LoadLibraryW(name: *const u16) -> Handle;
    fn FreeLibrary(module: Handle) -> Bool;
    fn GetProcAddress(module: Handle, name: *const c_char) -> *const c_void;
    fn GetCurrentThread() -> Handle;
}

unsafe extern "C" {
    fn os_utf8_to_wcs_ptr(str: *const c_char, len: usize, pstr: *mut *mut u16) -> usize;
    fn bfree(ptr: *mut c_void);
    /// MSVC `__try/__except` around `RaiseException(0x406D1388)`.
    /// MinGW builds skip it, as `threading-windows.c` does.
    #[cfg(target_env = "msvc")]
    fn obs_rust_raise_thread_name_exception(name: *const c_char);
}

/// ASCII `text` plus a UTF-16 NUL, for `LoadLibraryW`.
fn wide_literal(text: &str) -> Vec<u16> {
    text.encode_utf16().chain(core::iter::once(0)).collect()
}

/// `SetThreadDescription`, resolved at the call like the C.
type SetThreadDescription = unsafe extern "system" fn(Handle, *const u16) -> i32;

/// `os_event_t` on Windows: a manual or auto-reset Win32 event.
pub struct Event {
    handle: Handle,
    /// Threads that have entered `wait` / `timed_wait` and not left.
    /// Debug builds only, so a test can signal after every waiter is
    /// inside `WaitForSingleObject`.
    #[cfg(debug_assertions)]
    waiting: AtomicUsize,
}

// SAFETY: `SetEvent` / `ResetEvent` / `WaitForSingleObject` are
// thread-safe on one event handle. `Drop` (`CloseHandle`) is the
// caller's job to keep off those calls, same as the C.
unsafe impl Send for Event {}
// SAFETY: see `Send`.
unsafe impl Sync for Event {}

impl Event {
    /// `os_event_init`. Panics if `CreateEvent` fails; that is resource
    /// exhaustion, and the C returns -1 on the same path. The shim uses
    /// [`Event::try_new`] so it can return that -1.
    pub fn new(event_type: EventType) -> Self {
        Self::try_new(event_type == EventType::Manual)
            .unwrap_or_else(|| panic!("CreateEvent failed"))
    }

    pub(crate) fn try_new(manual: bool) -> Option<Self> {
        // SAFETY: a null name creates an unnamed event.
        let handle = unsafe { CreateEventW(ptr::null(), Bool::from(manual), 0, ptr::null()) };
        if handle.is_null() {
            return None;
        }
        Some(Self {
            handle,
            #[cfg(debug_assertions)]
            waiting: AtomicUsize::new(0),
        })
    }

    /// `os_event_signal`.
    pub fn signal(&self) {
        let _ = self.signal_code();
    }

    /// `os_event_signal`: 0, or `EINVAL` when `SetEvent` fails.
    pub(crate) fn signal_code(&self) -> c_int {
        // SAFETY: `handle` is an open event.
        if unsafe { SetEvent(self.handle) } == 0 {
            libc::EINVAL
        } else {
            0
        }
    }

    /// `os_event_reset`.
    pub fn reset(&self) {
        // SAFETY: `handle` is an open event.
        unsafe { ResetEvent(self.handle) };
    }

    /// `os_event_wait`. Blocks until the event is signalled. Matches
    /// the Unix `Event::wait`, which does not surface the wait code.
    /// The shim uses [`Self::wait_code`].
    pub fn wait(&self) {
        let _ = self.wait_code();
    }

    /// `os_event_wait`: 0, or `EINVAL` when the wait fails.
    pub(crate) fn wait_code(&self) -> c_int {
        self.wait_for(INFINITE)
    }

    /// `os_event_timedwait`. `true` when the event was signalled.
    pub fn timed_wait(&self, milliseconds: u64) -> bool {
        let ms = u32::try_from(milliseconds).unwrap_or(u32::MAX);
        self.wait_for(ms) == 0
    }

    /// `os_event_try`.
    pub fn try_wait(&self) -> bool {
        self.wait_for(0) == 0
    }

    /// Debug-build count of threads inside a wait. See [`Event::waiting`].
    #[cfg(debug_assertions)]
    #[doc(hidden)]
    pub fn waiting_count(&self) -> usize {
        self.waiting.load(Ordering::Acquire)
    }

    pub(crate) fn wait_for(&self, milliseconds: u32) -> c_int {
        #[cfg(debug_assertions)]
        self.waiting.fetch_add(1, Ordering::Release);
        // SAFETY: `handle` is an open event.
        let code = unsafe { WaitForSingleObject(self.handle, milliseconds) };
        #[cfg(debug_assertions)]
        self.waiting.fetch_sub(1, Ordering::Release);
        if code == WAIT_OBJECT_0 {
            0
        } else if code == WAIT_TIMEOUT {
            libc::ETIMEDOUT
        } else {
            libc::EINVAL
        }
    }
}

impl Drop for Event {
    fn drop(&mut self) {
        // SAFETY: `handle` came from `CreateEventW` and is closed once.
        unsafe { CloseHandle(self.handle) };
    }
}

/// `os_sem_t` on Windows: a counting semaphore capped at `0x7FFFFFFF`.
pub struct Sem {
    handle: Handle,
}

// SAFETY: `ReleaseSemaphore` and `WaitForSingleObject` are thread-safe
// on one semaphore. Destroying it while another thread waits is the
// caller's bug, as in the C.
unsafe impl Send for Sem {}
// SAFETY: see `Send`.
unsafe impl Sync for Sem {}

impl Sem {
    /// `os_sem_init` for a value that fits in a nonnegative `LONG`.
    /// `None` where `CreateSemaphore` fails.
    pub fn try_new(value: u32) -> Option<Pin<Box<Self>>> {
        let Ok(initial) = i32::try_from(value) else {
            return None;
        };
        // SAFETY: a null name creates an unnamed semaphore.
        let handle = unsafe { CreateSemaphoreW(ptr::null(), initial, SEM_MAX, ptr::null()) };
        if handle.is_null() {
            return None;
        }
        Some(Box::pin(Self { handle }))
    }

    /// Panics if [`Sem::try_new`] fails. A nonnegative value fails only
    /// on resource exhaustion.
    pub fn new(value: u32) -> Pin<Box<Self>> {
        Self::try_new(value).unwrap_or_else(|| panic!("os_sem_init({value}) failed"))
    }

    /// `os_sem_post`: 0, or -1 at the maximum count.
    pub fn post(&self) -> c_int {
        // SAFETY: `handle` is an open semaphore.
        if unsafe { ReleaseSemaphore(self.handle, 1, ptr::null_mut()) } == 0 {
            -1
        } else {
            0
        }
    }

    /// `os_sem_wait`: 0, or -1 when the wait fails.
    pub fn wait(&self) -> c_int {
        // SAFETY: `handle` is an open semaphore.
        let code = unsafe { WaitForSingleObject(self.handle, INFINITE) };
        if code == WAIT_OBJECT_0 { 0 } else { -1 }
    }

    /// Non-blocking take. Not in `util/threading.h`; the parity test uses
    /// it the way `sem_trywait` is used on Unix. `true` when a count was
    /// taken.
    pub fn try_wait(&self) -> bool {
        // SAFETY: `handle` is an open semaphore.
        unsafe { WaitForSingleObject(self.handle, 0) == WAIT_OBJECT_0 }
    }
}

impl Drop for Sem {
    fn drop(&mut self) {
        // SAFETY: `handle` came from `CreateSemaphoreW` and is closed once.
        unsafe { CloseHandle(self.handle) };
    }
}

/// `os_set_thread_name`: the debugger exception, then
/// `SetThreadDescription` of the UTF-16 name from `os_utf8_to_wcs_ptr`.
///
/// # Safety
/// `name` is a C string or null. Null is what the C passes through:
/// the exception carries a null name and `SetThreadDescription` is
/// called with a null wide string.
pub unsafe fn set_thread_name(name: *const c_char) {
    #[cfg(target_env = "msvc")]
    // SAFETY: the helper catches the exception the way the C `__except` does.
    unsafe {
        obs_rust_raise_thread_name_exception(name);
    }

    // SAFETY: `KernelBase.dll` is a constant wide name.
    let module = unsafe { LoadLibraryW(wide_literal("KernelBase.dll").as_ptr()) };
    if module.is_null() {
        return;
    }
    // SAFETY: `module` is a loaded library; the name is a C string.
    let set_desc: Option<SetThreadDescription> = unsafe {
        let sym = GetProcAddress(module, c"SetThreadDescription".as_ptr());
        if sym.is_null() {
            None
        } else {
            Some(core::mem::transmute::<*const c_void, SetThreadDescription>(
                sym,
            ))
        }
    };
    if let Some(set_desc) = set_desc {
        let mut wide: *mut u16 = ptr::null_mut();
        // SAFETY: `name` is a C string or null; a null `name` writes a null
        // `wide` and returns 0, as `os_utf8_to_wcs_ptr` does.
        unsafe { os_utf8_to_wcs_ptr(name, 0, &mut wide) };
        // SAFETY: `GetCurrentThread` is the calling thread. `wide` is the
        // converted name or null.
        unsafe { set_desc(GetCurrentThread(), wide) };
        // SAFETY: `wide` came from `bmalloc` inside `os_utf8_to_wcs_ptr`,
        // or is null. `bfree(NULL)` is a no-op.
        unsafe { bfree(wide.cast()) };
    }
    // SAFETY: `module` came from `LoadLibraryW`.
    unsafe { FreeLibrary(module) };
}
