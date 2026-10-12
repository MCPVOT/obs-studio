//! `util/threading-posix.c` on Unix and `util/threading-windows.c` on
//! Windows. Each wrapper renames the globals to `oracle_*`.
use core::ffi::{c_char, c_int, c_ulong};

/// `struct os_event_data`, opaque in `util/threading.h`.
pub enum OracleEvent {}

/// `struct os_sem_data`, opaque in `util/threading.h`.
pub enum OracleSem {}

unsafe extern "C" {
    pub fn oracle_os_event_init(event: *mut *mut OracleEvent, type_: c_int) -> c_int;
    pub fn oracle_os_event_destroy(event: *mut OracleEvent);
    pub fn oracle_os_event_wait(event: *mut OracleEvent) -> c_int;
    pub fn oracle_os_event_timedwait(event: *mut OracleEvent, ms: c_ulong) -> c_int;
    pub fn oracle_os_event_try(event: *mut OracleEvent) -> c_int;
    pub fn oracle_os_event_signal(event: *mut OracleEvent) -> c_int;
    pub fn oracle_os_event_reset(event: *mut OracleEvent);
    pub fn oracle_os_sem_init(sem: *mut *mut OracleSem, value: c_int) -> c_int;
    pub fn oracle_os_sem_destroy(sem: *mut OracleSem);
    pub fn oracle_os_sem_post(sem: *mut OracleSem) -> c_int;
    pub fn oracle_os_sem_wait(sem: *mut OracleSem) -> c_int;
    /// Oracle-side non-blocking take (`sem_trywait`, a zero-timeout
    /// `semaphore_timedwait` on macOS, or `WaitForSingleObject` with a
    /// zero timeout on Windows). Test probe only.
    pub fn oracle_os_sem_trywait(sem: *mut OracleSem) -> c_int;
    pub fn oracle_os_set_thread_name(name: *const c_char);
}
