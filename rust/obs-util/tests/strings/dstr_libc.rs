//! Tier 1: `libobs/util/dstr-libc.c` (obs-rust/obs-studio#1855).
//!
//! 1:1 port of `test/cmocka/test_dstr_libc.c` against the safe core and the
//! C ABI shims, plus `test_dstr_case` from `test_dstr.c`, plus edge cases:
//! signed `wchar_t` order, slices without a terminator, the four padding
//! characters only, and non-ASCII through the conversions. The cmocka leak
//! check is replaced by freeing every buffer with `bfree`.
//!
//! Case mapping is the C library's in the current locale; these tests use
//! ASCII, which every C locale maps the same way. Tier 3
//! (`dstr_libc_parity.rs`) compares arbitrary characters with the C.
//!
//! The variadic `dstr_printf`/`dstr_catf` stay C (`util/dstr-variadic.c`)
//! and are not tested here.
//!
//! No intentional differences from C.

use core::ffi::c_char;
use core::ptr;

use obs_c_oracle as _;
use obs_util::dstr_libc::{
    WChar, to_lower, to_upper, wcsdepad, wcslen, wstrcmp_n, wstrcmpi, wstrcmpi_n, wstrstri,
};
use obs_util::ffi::darray::bfree;
use obs_util::ffi::dstr::dstr;
use obs_util::ffi::dstr_libc as shim;

/// A NUL-terminated wide string of BMP text (the same on 16- and 32-bit
/// `wchar_t`).
fn w(s: &str) -> Vec<WChar> {
    s.encode_utf16().map(|u| u as WChar).chain([0]).collect()
}

fn new_dstr() -> dstr {
    dstr {
        array: ptr::null_mut(),
        len: 0,
        capacity: 0,
    }
}

fn copy(d: &mut dstr, s: &str) {
    let c = std::ffi::CString::new(s).unwrap();
    // SAFETY: `d` is a valid dstr; `c` is NUL-terminated.
    unsafe { obs_util::ffi::dstr::dstr_copy(d, c.as_ptr()) };
}

fn bytes(d: &dstr) -> &[u8] {
    if d.array.is_null() {
        return &[];
    }
    // SAFETY: a non-null array holds `len` bytes.
    unsafe { core::slice::from_raw_parts(d.array.cast::<u8>(), d.len) }
}

fn free(d: &mut dstr) {
    // SAFETY: `array` is null or from bmalloc.
    unsafe { bfree(d.array.cast()) };
    *d = new_dstr();
}

#[test]
fn test_wstrcmpi() {
    assert_eq!(wstrcmpi(&w("Hello"), &w("hELLO")), 0);
    assert_eq!(wstrcmpi(&w("abc"), &w("ABD")), -1);
    assert_eq!(wstrcmpi(&w("abd"), &w("ABC")), 1);
    assert_eq!(wstrcmpi(&w("ab"), &w("abc")), -1);
    assert_eq!(wstrcmpi(&w("abc"), &w("ab")), 1);
    assert_eq!(wstrcmpi(&w("a"), &w("_")), -1);
    // NULL is the empty string (shim)
    let a = w("a");
    let empty = w("");
    // SAFETY: null or NUL-terminated wide strings.
    unsafe {
        assert_eq!(shim::wstrcmpi(ptr::null(), ptr::null()), 0);
        assert_eq!(shim::wstrcmpi(ptr::null(), empty.as_ptr()), 0);
        assert_eq!(shim::wstrcmpi(empty.as_ptr(), ptr::null()), 0);
        assert_eq!(shim::wstrcmpi(ptr::null(), a.as_ptr()), -1);
        assert_eq!(shim::wstrcmpi(a.as_ptr(), ptr::null()), 1);
    }
}

#[test]
fn test_wstrcmp_n() {
    assert_eq!(wstrcmp_n(&w("abc"), &w("abd"), 2), 0);
    assert_eq!(wstrcmp_n(&w("abc"), &w("abd"), 3), -1);
    assert_eq!(wstrcmp_n(&w("abd"), &w("abc"), 3), 1);
    assert_eq!(wstrcmp_n(&w("abc"), &w("ABC"), 3), 1);
    assert_eq!(wstrcmp_n(&w("abc"), &w("abc"), 100), 0);
    assert_eq!(wstrcmp_n(&w("ab"), &w("abc"), 100), -1);
    assert_eq!(wstrcmp_n(&w("a"), &w("b"), 0), 0);
    let b = w("b");
    // SAFETY: null or NUL-terminated wide strings.
    unsafe {
        assert_eq!(shim::wstrcmp_n(ptr::null(), b.as_ptr(), 0), 0);
        assert_eq!(shim::wstrcmp_n(ptr::null(), ptr::null(), 5), 0);
        assert_eq!(shim::wstrcmp_n(ptr::null(), b.as_ptr(), 5), -1);
        assert_eq!(shim::wstrcmp_n(b.as_ptr(), ptr::null(), 5), 1);
    }
}

#[test]
fn test_wstrcmpi_n() {
    assert_eq!(wstrcmpi_n(&w("ABc"), &w("abD"), 2), 0);
    assert_eq!(wstrcmpi_n(&w("ABc"), &w("abD"), 3), -1);
    assert_eq!(wstrcmpi_n(&w("abD"), &w("ABc"), 3), 1);
    assert_eq!(wstrcmpi_n(&w("Hello"), &w("hello"), 100), 0);
    assert_eq!(wstrcmpi_n(&w("a"), &w("b"), 0), 0);
    let x = w("x");
    // SAFETY: null or NUL-terminated wide strings.
    unsafe {
        assert_eq!(shim::wstrcmpi_n(ptr::null(), ptr::null(), 3), 0);
        assert_eq!(shim::wstrcmpi_n(ptr::null(), x.as_ptr(), 3), -1);
        assert_eq!(shim::wstrcmpi_n(x.as_ptr(), ptr::null(), 3), 1);
    }
}

#[test]
fn test_wstrstri() {
    let hay = w("Hello World");
    assert_eq!(wstrstri(&hay, &w("WORLD")), Some(6));
    assert_eq!(wstrstri(&hay, &w("lo w")), Some(3));
    assert_eq!(wstrstri(&hay, &w("hello")), Some(0));
    assert_eq!(wstrstri(&hay, &w("worlds")), None);
    assert_eq!(wstrstri(&hay, &w("xyz")), None);
    assert_eq!(wstrstri(&hay, &w("")), Some(0));
    assert_eq!(wstrstri(&w(""), &w("")), Some(0));
    let a = w("a");
    // SAFETY: null or NUL-terminated wide strings.
    unsafe {
        assert_eq!(shim::wstrstri(hay.as_ptr(), a.as_ptr()), ptr::null_mut());
        assert_eq!(
            shim::wstrstri(hay.as_ptr(), w("WORLD").as_ptr()),
            hay.as_ptr().add(6).cast_mut()
        );
        assert!(shim::wstrstri(ptr::null(), a.as_ptr()).is_null());
        assert!(shim::wstrstri(hay.as_ptr(), ptr::null()).is_null());
    }
}

#[test]
fn test_wcsdepad() {
    let mut buf = w(" \t\r\nab c \n\t\r ");
    wcsdepad(&mut buf);
    assert_eq!(&buf[..=wcslen(&buf)], &w("ab c")[..]);

    let mut buf = w("abc");
    wcsdepad(&mut buf);
    assert_eq!(buf, w("abc"));

    // only the four padding characters: a vertical tab stays
    let mut buf = w("\u{b}ab\u{b}");
    wcsdepad(&mut buf);
    assert_eq!(buf, w("\u{b}ab\u{b}"));

    let mut buf = w(" \t \n");
    wcsdepad(&mut buf);
    assert_eq!(buf[0], 0);

    let mut buf = w("");
    wcsdepad(&mut buf);
    assert_eq!(buf[0], 0);

    // the shim returns its argument, and NULL for NULL
    let mut buf = w("  x ");
    // SAFETY: a writable NUL-terminated wide string, then NULL.
    unsafe {
        assert_eq!(shim::wcsdepad(buf.as_mut_ptr()), buf.as_mut_ptr());
        assert!(shim::wcsdepad(ptr::null_mut()).is_null());
    }
    assert_eq!(&buf[..2], &w("x")[..]);
}

#[test]
fn compares_signed_like_wchar_t() {
    // Compares are on wchar_t values, as C's `<`. A value above every
    // ASCII letter sorts after them where wchar_t is unsigned or 16-bit,
    // and so does 0x7fff_ffff on 32-bit wchar_t.
    let high = [0x7fff as WChar, 0];
    assert_eq!(wstrcmp_n(&w("z"), &high, 1), -1);
    assert_eq!(wstrcmp_n(&high, &w("z"), 1), 1);
}

#[test]
fn slices_without_a_terminator_end_at_their_length() {
    let a: Vec<WChar> = w("abc")[..3].to_vec();
    assert_eq!(wcslen(&a), 3);
    assert_eq!(wstrcmpi(&a, &w("ABC")), 0);
    assert_eq!(wstrcmp_n(&a, &w("abcd"), 10), -1);
    assert_eq!(wstrstri(&a, &w("C")), Some(2));
    assert_eq!(wstrstri(&a, &w("cd")), None);
    let mut b: Vec<WChar> = w(" ab ")[..4].to_vec();
    wcsdepad(&mut b);
    assert_eq!(&b[..2], &w("ab")[..2]);
    let mut c: Vec<WChar> = w("ab")[..2].to_vec();
    to_upper(&mut c);
    assert_eq!(c, w("AB")[..2].to_vec());
    to_lower(&mut c);
    assert_eq!(c, w("ab")[..2].to_vec());
    assert_eq!(wcslen(&[]), 0);
}

#[test]
fn case_maps_stop_at_the_terminator() {
    let mut s = w("aB1_zZ");
    s.extend(w("tail"));
    to_upper(&mut s);
    assert_eq!(&s[..7], &w("AB1_ZZ")[..]);
    assert_eq!(&s[7..], &w("tail")[..]);
    to_lower(&mut s);
    assert_eq!(&s[..7], &w("ab1_zz")[..]);
}

#[test]
fn test_dstr_wcs() {
    // UTF-8 with 2-, 3- and 4-byte sequences
    let utf8 = "h\u{e9}llo \u{20ac} \u{1f600}";
    let mut d = new_dstr();
    copy(&mut d, utf8);
    // SAFETY: valid dstrs; the wide string comes from bmalloc.
    unsafe {
        let wide = shim::dstr_to_wcs(&d);
        assert!(!wide.is_null());
        assert_eq!(*wide, 'h' as WChar);
        assert_eq!(*wide.add(1), 0xe9 as WChar);
        assert_eq!(*wide.add(6), 0x20ac as WChar);
        free(&mut d);
        shim::dstr_from_wcs(&mut d, wide);
        assert_eq!(bytes(&d), utf8.as_bytes());
        assert_eq!(d.len, utf8.len());
        assert!(d.capacity > d.len);
        bfree(wide.cast());
        free(&mut d);

        // an empty wide string frees the destination
        copy(&mut d, "old");
        let empty = w("");
        shim::dstr_from_wcs(&mut d, empty.as_ptr());
        assert!(d.array.is_null());
        assert_eq!((d.len, d.capacity), (0, 0));

        // an empty dstr (NULL array) converts to NULL
        assert!(shim::dstr_to_wcs(&d).is_null());
        assert!(shim::dstr_to_mbs(&d).is_null());
    }
}

#[test]
fn test_dstr_mbs() {
    let mut d = new_dstr();
    copy(&mut d, "old");
    // SAFETY: valid dstrs and NUL-terminated strings.
    unsafe {
        shim::dstr_from_mbs(&mut d, c"plain ascii".as_ptr());
        assert_eq!(bytes(&d), b"plain ascii");
        assert_eq!(d.len, 11);
        let m = shim::dstr_to_mbs(&d);
        assert_eq!(core::ffi::CStr::from_ptr(m).to_bytes(), b"plain ascii");
        bfree(m.cast());
        free(&mut d);

        copy(&mut d, "old");
        shim::dstr_from_mbs(&mut d, ptr::null::<c_char>());
        assert!(d.array.is_null());
        assert_eq!(d.len, 0);
    }
}

#[test]
fn test_dstr_case() {
    let mut d = new_dstr();
    // SAFETY: valid dstrs.
    unsafe {
        // empty is a no-op
        shim::dstr_to_upper(&mut d);
        shim::dstr_to_lower(&mut d);
        assert!(d.array.is_null());

        copy(&mut d, "Hello, World 123");
        shim::dstr_to_upper(&mut d);
        assert_eq!(bytes(&d), b"HELLO, WORLD 123");
        assert_eq!(d.len, 16);
        assert!(d.capacity > d.len);
        shim::dstr_to_lower(&mut d);
        assert_eq!(bytes(&d), b"hello, world 123");
        assert_eq!(d.len, 16);
        free(&mut d);

        // a dstr whose first byte is NUL counts as empty and is not touched
        copy(&mut d, "x");
        *d.array = 0;
        shim::dstr_to_upper(&mut d);
        assert_eq!(d.len, 1);
        assert_eq!(*d.array, 0);
        free(&mut d);

        copy(&mut d, "MiXeD 09_az{}");
        shim::dstr_to_upper(&mut d);
        assert_eq!(bytes(&d), b"MIXED 09_AZ{}");
        shim::dstr_to_lower(&mut d);
        assert_eq!(bytes(&d), b"mixed 09_az{}");
        assert_eq!(d.len, 13);
        free(&mut d);

        // non-ASCII passes through the conversions unchanged in byte count
        copy(&mut d, "a\u{20ac}b");
        shim::dstr_to_upper(&mut d);
        assert_eq!(&bytes(&d)[..1], b"A");
        assert_eq!(&bytes(&d)[d.len - 1..], b"B");
        free(&mut d);
    }
}
