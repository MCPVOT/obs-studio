//! Safe core for the Windows branch of `libobs/util/utf8.c`.
//!
//! The conversion itself is Windows's: `MultiByteToWideChar` and
//! `WideCharToMultiByte` with `CP_UTF8` and no flags, called with the same
//! arguments as the C. So invalid UTF-8 and lone surrogates become U+FFFD
//! exactly as in C, and a buffer that is too small fails the same way (0,
//! with part of the buffer possibly written).
//!
//! `flags` do nothing here, as in C: `UTF8_IGNORE_ERROR` and `UTF8_SKIP_BOM`
//! belong to the other branch ([`crate::utf8`]). A leading UTF-8 BOM is always
//! dropped from the input of [`utf8_to_wchar`] instead.
//!
//! Intentional difference: C casts the sizes to `int`. A length above
//! `i32::MAX` gives 0 here instead of reaching the API as a negative size.

use core::ptr;

const CP_UTF8: u32 = 65001;
const UTF8_BOM: &[u8] = &[0xef, 0xbb, 0xbf];

// Declared against kernel32.lib, like `crate::platform::windows::pipe`: windows-sys's
// raw-dylib import records collide with std's in obs.dll (LNK2005).
#[link(name = "kernel32")]
#[allow(non_snake_case)]
unsafe extern "system" {
    fn MultiByteToWideChar(
        code_page: u32,
        flags: u32,
        multi_byte: *const u8,
        multi_byte_len: i32,
        wide: *mut u16,
        wide_len: i32,
    ) -> i32;
    fn WideCharToMultiByte(
        code_page: u32,
        flags: u32,
        wide: *const u16,
        wide_len: i32,
        multi_byte: *mut u8,
        multi_byte_len: i32,
        default_char: *const u8,
        used_default_char: *mut i32,
    ) -> i32;
}

/// The output arguments for the API: a null pointer and 0 ask for the size;
/// a buffer is passed with its length (0 also asks for the size). `None` if
/// the length does not fit an `int`.
fn out_args<T>(out: Option<&mut [T]>) -> Option<(*mut T, i32)> {
    match out {
        None => Some((ptr::null_mut(), 0)),
        Some(buf) => i32::try_from(buf.len())
            .ok()
            .map(|len| (buf.as_mut_ptr(), len)),
    }
}

/// The API's result as C returns it: the count, or 0 on failure.
fn count(ret: i32) -> usize {
    usize::try_from(ret).unwrap_or(0)
}

/// `utf8_to_wchar`: converts `input` (UTF-8) to UTF-16 in `out`, or returns
/// the number of units needed when `out` is `None` or empty. A leading UTF-8
/// BOM is dropped first. Returns 0 on failure, including empty input and a
/// buffer that is too small.
pub fn utf8_to_wchar(input: &[u8], out: Option<&mut [u16]>) -> usize {
    // C: "prevent bom from being used in the string"
    let input = input.strip_prefix(UTF8_BOM).unwrap_or(input);
    let (Ok(len), Some((out, out_len))) = (i32::try_from(input.len()), out_args(out)) else {
        return 0;
    };
    // SAFETY: `input` covers `len` bytes; `out` is null with 0, or covers
    // `out_len` units.
    count(unsafe { MultiByteToWideChar(CP_UTF8, 0, input.as_ptr(), len, out, out_len) })
}

/// `wchar_to_utf8`: converts `input` (UTF-16) to UTF-8 in `out`, or returns
/// the number of bytes needed when `out` is `None` or empty. Returns 0 on
/// failure, including empty input and a buffer that is too small.
pub fn wchar_to_utf8(input: &[u16], out: Option<&mut [u8]>) -> usize {
    let (Ok(len), Some((out, out_len))) = (i32::try_from(input.len()), out_args(out)) else {
        return 0;
    };
    // SAFETY: `input` covers `len` units; `out` is null with 0, or covers
    // `out_len` bytes; no default character is used, as in C.
    count(unsafe {
        WideCharToMultiByte(
            CP_UTF8,
            0,
            input.as_ptr(),
            len,
            out,
            out_len,
            ptr::null(),
            ptr::null_mut(),
        )
    })
}
