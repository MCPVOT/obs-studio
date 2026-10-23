//! Safe core of `libobs/util/dstr-libc.c`: the wide-string compares, search
//! and depad, and the case mapping `dstr_to_upper`/`dstr_to_lower` apply.
//!
//! Strings are slices of the host `wchar_t` ([`WChar`]) that end at their
//! first 0. A slice without a 0 reads as if one followed its last element,
//! so no input can index out of bounds. Comparisons are on `wchar_t` values,
//! signed where the host's `wchar_t` is, as in C. Case mapping is the C
//! library's `towupper`/`towlower` in the current locale (through
//! [`crate::platform::crt`]), so it matches libobs in any locale.
//!
//! The C variadic `dstr_printf`/`dstr_catf`/`dstr_vprintf`/`dstr_vcatf`
//! stay C (`util/dstr-variadic.c`); the conversions to and from multibyte
//! and wide strings are in the C ABI shim ([`crate::ffi::dstr_libc`]), over
//! `util/platform-conversions.c` and the ported `utf8`.

pub use crate::platform::WChar;

use crate::platform::crt;

/// The character at `i`, or 0 past the end of the slice.
fn at(s: &[WChar], i: usize) -> WChar {
    s.get(i).copied().unwrap_or(0)
}

/// The string from `i` on (empty past the end).
fn from(s: &[WChar], i: usize) -> &[WChar] {
    s.get(i..).unwrap_or(&[])
}

/// Length up to the first 0 (`wcslen`), or the slice length if it has none.
pub fn wcslen(s: &[WChar]) -> usize {
    s.iter().position(|&c| c == 0).unwrap_or(s.len())
}

fn identity(ch: WChar) -> WChar {
    ch
}

/// The shared loop of the three compares, step for step:
///
/// ```c
/// do {
///     ch1 = map(*str1); ch2 = map(*str2);
///     if (ch1 < ch2) return -1; else if (ch1 > ch2) return 1;
/// } while (*str1++ && *str2++ && --n);
/// ```
///
/// `n` is `None` for the unbounded `wstrcmpi`.
fn compare(str1: &[WChar], str2: &[WChar], mut n: Option<usize>, map: fn(WChar) -> WChar) -> i32 {
    if n == Some(0) {
        return 0;
    }
    let mut i = 0;
    loop {
        let ch1 = map(at(str1, i));
        let ch2 = map(at(str2, i));
        if ch1 < ch2 {
            return -1;
        } else if ch1 > ch2 {
            return 1;
        }
        if at(str1, i) == 0 || at(str2, i) == 0 {
            return 0;
        }
        if let Some(left) = n.as_mut() {
            *left -= 1;
            if *left == 0 {
                return 0;
            }
        }
        i += 1;
    }
}

/// `wstrcmpi`: compare after `towupper`; -1, 0 or 1.
pub fn wstrcmpi(str1: &[WChar], str2: &[WChar]) -> i32 {
    compare(str1, str2, None, crt::towupper)
}

/// `wstrcmp_n`: compare at most `n` characters; -1, 0 or 1.
pub fn wstrcmp_n(str1: &[WChar], str2: &[WChar], n: usize) -> i32 {
    compare(str1, str2, Some(n), identity)
}

/// `wstrcmpi_n`: [`wstrcmp_n`] after `towupper`.
pub fn wstrcmpi_n(str1: &[WChar], str2: &[WChar], n: usize) -> i32 {
    compare(str1, str2, Some(n), crt::towupper)
}

/// `wstrstri`: the offset of the first case-insensitive match of `find`
/// in `str`, or `None`. Every offset up to and including the terminator is
/// tried, so an empty `find` matches at 0.
pub fn wstrstri(str: &[WChar], find: &[WChar]) -> Option<usize> {
    let len = wcslen(find);
    let mut i = 0;
    loop {
        if wstrcmpi_n(from(str, i), find, len) == 0 {
            return Some(i);
        }
        if at(str, i) == 0 {
            return None;
        }
        i += 1;
    }
}

/// `libc_is_padding`: space, tab, LF or CR, compared as C `int`.
fn is_padding(ch: WChar) -> bool {
    matches!(i64::from(ch), 0x20 | 0x09 | 0x0a | 0x0d)
}

/// `wcsdepad`: strip leading and trailing space, tab, CR and LF in place.
/// The string moves to the front of the slice with its terminator (when
/// the slice has one); trailing padding is overwritten with 0.
pub fn wcsdepad(str: &mut [WChar]) {
    if at(str, 0) == 0 {
        return;
    }
    let mut start = 0;
    while is_padding(at(str, start)) {
        start += 1;
    }
    let len = wcslen(from(str, start));
    if start != 0 {
        let end = (start + len + 1).min(str.len());
        str.copy_within(start..end, 0);
    }
    if len != 0 {
        let mut i = len - 1;
        while is_padding(str[i]) {
            str[i] = 0;
            // str[0] is not padding (leading padding was removed above).
            i -= 1;
        }
    }
}

fn map_in_place(str: &mut [WChar], map: fn(WChar) -> WChar) {
    for ch in str.iter_mut().take_while(|ch| **ch != 0) {
        *ch = map(*ch);
    }
}

/// The `dstr_to_upper` loop: `towupper` every character before the first 0.
pub fn to_upper(str: &mut [WChar]) {
    map_in_place(str, crt::towupper);
}

/// The `dstr_to_lower` loop: `towlower` every character before the first 0.
pub fn to_lower(str: &mut [WChar]) {
    map_in_place(str, crt::towlower);
}
