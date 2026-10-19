//! C ABI for `proc_handler_create`, `proc_handler_destroy`,
//! `proc_handler_add`, and `proc_handler_call`.
//!
//! `proc_handler_t` is opaque in `proc.h`. The pointer is a
//! [`ProcHandler`]. Declaration text is parsed by the C
//! `parse_decl_string` (still in `callback/decl.c.h`). The name it
//! allocates is copied into the table and released with `bfree`, which is
//! what `decl_info_free` does. The declaration string itself belongs to
//! the caller and is not freed.
//!
//! The handler object is a `Box`. C uses `bmalloc` for that object.
//! `bnum_allocs` still balances: the test counts `bmalloc` only, and the
//! declaration strings are the allocations that remain.
//!
//! A null name on an empty handler returns false. Once any procedure is
//! registered, a null name faults in `strcmp`. A stored null callback
//! faults when called. Both match `proc.c.h`. The safe core does not
//! fault on either.
//!
//! `pthread_mutex_init_recursive` can fail and C then returns NULL. The
//! Rust mutex does not fail that way.

use std::ffi::{CStr, c_char, c_int, c_void};

use crate::proc::{AddOutcome, PreparedCall, ProcHandler};

const LOG_ERROR: c_int = 100;
const LOG_WARNING: c_int = 200;

const INVALID_DECL: &CStr = c"Function declaration invalid: %s";
const ALREADY_EXISTS: &CStr = c"Procedure '%s' already exists";

type ProcFn = unsafe extern "C" fn(*mut c_void, *mut c_void);

unsafe extern "C" {
    fn parse_decl_string(decl: *mut DeclInfo, decl_string: *const c_char) -> bool;
    fn bfree(ptr: *mut c_void);
    fn blog(level: c_int, format: *const c_char, ...);
    fn strcmp(left: *const c_char, right: *const c_char) -> c_int;
}

/// `struct decl_param` in `libobs/callback/decl.h`.
#[repr(C)]
pub struct DeclParam {
    pub name: *mut c_char,
    pub type_: i32,
    pub flags: u32,
}

/// `struct darray` in `libobs/util/darray.h`.
#[repr(C)]
pub struct Darray {
    pub array: *mut c_void,
    pub num: usize,
    pub capacity: usize,
}

/// `struct decl_info` in `libobs/callback/decl.h`.
#[repr(C)]
pub struct DeclInfo {
    pub name: *mut c_char,
    pub decl_string: *const c_char,
    pub params: Darray,
}

fn blank_decl() -> DeclInfo {
    DeclInfo {
        name: std::ptr::null_mut(),
        decl_string: std::ptr::null(),
        params: Darray {
            array: std::ptr::null_mut(),
            num: 0,
            capacity: 0,
        },
    }
}

/// # Safety
///
/// `decl` was zeroed, then filled by `parse_decl_string` on success. Each
/// param name and `decl->name` is null or a `bmalloc` block. `params.array`
/// is null or a `bmalloc` block of `num` params. `decl_string` is not freed.
unsafe fn free_decl(decl: &mut DeclInfo) {
    if !decl.params.array.is_null() && decl.params.num > 0 {
        // SAFETY: the parser allocated `num` params with `bmalloc`.
        let params = unsafe {
            std::slice::from_raw_parts(decl.params.array.cast::<DeclParam>(), decl.params.num)
        };
        for param in params {
            if !param.name.is_null() {
                unsafe { bfree(param.name.cast()) };
            }
        }
    }
    if !decl.params.array.is_null() {
        unsafe { bfree(decl.params.array) };
    }
    if !decl.name.is_null() {
        unsafe { bfree(decl.name.cast()) };
    }
    *decl = blank_decl();
}

/// # Safety
///
/// `name` is null or a NUL-terminated string. The returned bytes omit the
/// trailing NUL, which is what `strcmp` compares.
unsafe fn name_bytes(name: *const c_char) -> Vec<u8> {
    if name.is_null() {
        Vec::new()
    } else {
        // SAFETY: the caller guarantees a NUL-terminated string.
        unsafe { CStr::from_ptr(name) }.to_bytes().to_vec()
    }
}

/// # Safety
///
/// The returned pointer is null, or it came from [`proc_handler_create`]
/// and has not been destroyed.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn proc_handler_create() -> *mut ProcHandler {
    Box::into_raw(Box::new(ProcHandler::new()))
}

/// # Safety
///
/// `handler` is null, or it came from [`proc_handler_create`] and this is
/// the only destroy. The callback that calls this must be the last use of
/// the pointer on that call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn proc_handler_destroy(handler: *mut ProcHandler) {
    if handler.is_null() {
        return;
    }
    // SAFETY: the caller guarantees this is the owning pointer.
    drop(unsafe { Box::from_raw(handler) });
}

/// # Safety
///
/// `handler` is null or a live handler. `decl_string` is null or
/// NUL-terminated and stays valid for the call. `proc` is a callback or
/// null. `data` is an opaque pointer passed back to the callback.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn proc_handler_add(
    handler: *mut ProcHandler,
    decl_string: *const c_char,
    proc: Option<ProcFn>,
    data: *mut c_void,
) {
    let Some(handler) = (unsafe { handler.as_ref() }) else {
        return;
    };
    let mut decl = blank_decl();
    // SAFETY: `decl` is zeroed and writable for the parser. `decl_string`
    // is null or NUL-terminated.
    let parsed = unsafe { parse_decl_string(&mut decl, decl_string) };
    if !parsed {
        // SAFETY: the format takes one `%s`. A null `decl_string` is what
        // C prints as `(null)`. The parser already freed `decl` on failure.
        unsafe { blog(LOG_ERROR, INVALID_DECL.as_ptr(), decl_string) };
        return;
    }
    // SAFETY: a successful parse stored a NUL-terminated name.
    let name = unsafe { name_bytes(decl.name) };
    let func = proc.map(|callback| callback as usize).unwrap_or(0);
    if matches!(handler.add(&name, func, data as usize), AddOutcome::Exists) {
        // SAFETY: `decl.name` is still the parser's block. The format takes
        // one `%s`. The lock is already released; C logs while holding its
        // recursive mutex. The stored callback is the same either way.
        unsafe { blog(LOG_WARNING, ALREADY_EXISTS.as_ptr(), decl.name) };
    }
    // SAFETY: the parse succeeded, so the name and param blocks are live.
    unsafe { free_decl(&mut decl) };
}

/// # Safety
///
/// `handler` is null or a live handler. `name` is null or NUL-terminated.
/// `params` is null or a `calldata_t` the callback may read. A null `name`
/// once a procedure is registered faults in `strcmp`. A stored null
/// callback faults when this calls it. Do not use `handler` after the
/// callback returns: the callback may have destroyed it.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn proc_handler_call(
    handler: *mut ProcHandler,
    name: *const c_char,
    params: *mut c_void,
) -> bool {
    let prepared = {
        let Some(handler) = (unsafe { handler.as_ref() }) else {
            return false;
        };
        if name.is_null() {
            if handler.is_empty() {
                return false;
            }
            // SAFETY: the right-hand pointer is null on purpose. `strcmp`
            // faults, which is what `getproc` does once any proc exists.
            unsafe { strcmp(c"registered".as_ptr(), name) };
            return false;
        }
        // SAFETY: `name` is NUL-terminated. The borrow of `handler` ends
        // with this block, before the callback runs.
        let bytes = unsafe { CStr::from_ptr(name) }.to_bytes();
        handler.prepare_call(Some(bytes))
    };
    let PreparedCall::Ready { func, data } = prepared else {
        return false;
    };
    let callback: ProcFn = unsafe { std::mem::transmute(func) };
    unsafe { callback(data as *mut c_void, params) };
    true
}
