//! C ABI for `calldata_get_data`, `calldata_set_data`, and
//! `calldata_get_string`.
//!
//! The header inlines (`calldata_init`, `calldata_clear`, `calldata_free`,
//! the typed helpers) stay in `calldata.h`. Stack memory is `bmalloc` /
//! `brealloc` / `bfree`, because `calldata_free` releases a non-fixed stack
//! with `bfree`.
//!
//! Intentional differences, all where C reads or writes through a null
//! pointer: a null `out` or string out-pointer returns false, and a null
//! stack with a non-zero size is not found. API-built stacks do neither.
//! A walk that runs past the used size is not found; C would read off the
//! end. Stacks built through this API stay in range.

use std::ffi::{CStr, c_char, c_int, c_void};

use crate::calldata::{Calldata, FIXED_OVERFLOW_MESSAGE, StringGet};

const LOG_ERROR: c_int = 100;

/// Same text as [`FIXED_OVERFLOW_MESSAGE`]. The cmocka test matches this
/// string exactly.
const FIXED_OVERFLOW: &CStr = c"Tried to go above fixed calldata stack size!";

unsafe extern "C" {
    fn bmalloc(size: usize) -> *mut c_void;
    fn brealloc(ptr: *mut c_void, size: usize) -> *mut c_void;
    fn blog(level: c_int, fmt: *const c_char, ...);
}

/// Layout of `struct calldata` in `libobs/callback/calldata.h`.
#[repr(C)]
pub struct calldata {
    pub stack: *mut u8,
    pub size: usize,
    pub capacity: usize,
    pub fixed: bool,
}

/// # Safety
///
/// `data` is null or a valid `calldata`. `name` is null or NUL-terminated.
/// `out` is null or writable for `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn calldata_get_data(
    data: *const calldata,
    name: *const c_char,
    out: *mut c_void,
    size: usize,
) -> bool {
    let Some(data) = (unsafe { data.as_ref() }) else {
        return false;
    };
    let Some(name) = (unsafe { name_bytes(name) }) else {
        return false;
    };
    if data.stack.is_null() {
        return false;
    }
    // SAFETY: `stack` addresses `capacity` bytes from bmalloc or the caller.
    let bytes = unsafe { std::slice::from_raw_parts(data.stack, data.capacity) };
    let copied = Calldata::from_copy(bytes, data.size, true);
    let Some(got) = copied.get_bytes(name, size) else {
        return false;
    };
    if size == 0 {
        return true;
    }
    if out.is_null() {
        return false;
    }
    // SAFETY: `out` is writable for `size` bytes, and `got` has that length.
    unsafe { std::ptr::copy_nonoverlapping(got.as_ptr(), out.cast::<u8>(), size) };
    true
}

/// # Safety
///
/// `data` is null or a valid `calldata`. `name` is null or NUL-terminated.
/// `input` is null only when `size` is 0; otherwise it is readable for
/// `size` bytes.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn calldata_set_data(
    data: *mut calldata,
    name: *const c_char,
    input: *const c_void,
    size: usize,
) {
    let Some(data) = (unsafe { data.as_mut() }) else {
        return;
    };
    let Some(name) = (unsafe { name_bytes(name) }) else {
        return;
    };
    if size > 0 && input.is_null() {
        return;
    }
    let input = if size == 0 {
        &[][..]
    } else {
        // SAFETY: `input` is readable for `size` bytes.
        unsafe { std::slice::from_raw_parts(input.cast::<u8>(), size) }
    };

    let mut cd = if data.stack.is_null() {
        Calldata::new()
    } else {
        // SAFETY: `stack` addresses `capacity` bytes.
        let bytes = unsafe { std::slice::from_raw_parts(data.stack, data.capacity) };
        Calldata::from_copy(bytes, data.size, data.fixed)
    };
    let before = cd.overflow_count();
    cd.set_bytes(name, input);
    if cd.overflow_count() != before {
        // SAFETY: the format is a literal with no conversions.
        unsafe { blog(LOG_ERROR, FIXED_OVERFLOW.as_ptr()) };
        return;
    }
    commit(data, &cd);
}

/// # Safety
///
/// `data` is null or a valid `calldata`. `name` is null or NUL-terminated.
/// `out` is null or writable. On success the pointer written to `out` points
/// into `data->stack` and lives until the next set, clear, or free.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn calldata_get_string(
    data: *const calldata,
    name: *const c_char,
    out: *mut *const c_char,
) -> bool {
    if out.is_null() {
        return false;
    }
    let Some(data) = (unsafe { data.as_ref() }) else {
        return false;
    };
    let Some(name) = (unsafe { name_bytes(name) }) else {
        return false;
    };
    if data.stack.is_null() {
        return false;
    }
    // SAFETY: `stack` addresses `capacity` bytes.
    let bytes = unsafe { std::slice::from_raw_parts(data.stack, data.capacity) };
    let copied = Calldata::from_copy(bytes, data.size, true);
    match copied.string_at(name) {
        StringGet::Missing => false,
        StringGet::Null => {
            // SAFETY: `out` is writable.
            unsafe { *out = std::ptr::null() };
            true
        }
        StringGet::Data(found) => {
            let base = copied.stack().expect("copy has the stack").as_ptr() as usize;
            let offset = found.as_ptr() as usize - base;
            // SAFETY: `offset` is inside the original stack, which has the
            // same bytes as the copy. `out` is writable.
            unsafe { *out = data.stack.add(offset).cast() };
            true
        }
    }
}

/// # Safety
///
/// `name` is null or a NUL-terminated string that stays valid for the
/// returned borrow.
unsafe fn name_bytes<'a>(name: *const c_char) -> Option<&'a [u8]> {
    if name.is_null() {
        return None;
    }
    // SAFETY: the caller guarantees a NUL-terminated string.
    let bytes = unsafe { CStr::from_ptr(name) }.to_bytes();
    if bytes.is_empty() { None } else { Some(bytes) }
}

fn commit(data: &mut calldata, cd: &Calldata<'_>) {
    let Some(bytes) = cd.stack() else {
        return;
    };
    let new_cap = cd.capacity();
    if data.fixed {
        // SAFETY: the caller buffer has `capacity` bytes and the copy has
        // the same length. Fixed writes do not grow it.
        unsafe { std::ptr::copy_nonoverlapping(bytes.as_ptr(), data.stack, bytes.len()) };
        data.size = cd.size();
        return;
    }
    if data.stack.is_null() || new_cap != data.capacity {
        // SAFETY: a fresh stack is `bmalloc`; a grown one is `brealloc` of
        // the previous `bmalloc` pointer. Both match `calldata.c.h`.
        let ptr = unsafe {
            if data.stack.is_null() {
                bmalloc(new_cap)
            } else {
                brealloc(data.stack.cast(), new_cap)
            }
        };
        data.stack = ptr.cast();
        data.capacity = new_cap;
    }
    // SAFETY: `data.stack` now addresses `new_cap` bytes, and `bytes` is
    // that long.
    unsafe { std::ptr::copy_nonoverlapping(bytes.as_ptr(), data.stack, bytes.len()) };
    data.size = cd.size();
}

const fn overflow_text_matches() {
    let left = FIXED_OVERFLOW_MESSAGE.as_bytes();
    let right = FIXED_OVERFLOW.to_bytes();
    let mut i = 0;
    while i < left.len() {
        assert!(i < right.len() && left[i] == right[i]);
        i += 1;
    }
    assert!(left.len() == right.len());
}

const _: () = overflow_text_matches();
