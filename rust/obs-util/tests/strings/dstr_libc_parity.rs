//! Tier 3: the Rust `dstr-libc.c` shims agree with the original C
//! (`util/dstr-libc.c`, compiled into the oracle as `oracle_*` by
//! `oracle/dstr_libc.c`) on arbitrary input (obs-rust/obs-studio#1855).
//!
//! Wide strings mix ASCII, the padding characters and their near misses
//! (`\v`, `\f`), cased Latin-1 and Greek letters, and any value up to
//! U+10FFFF (truncated to 16 bits where `wchar_t` is), so the compares see
//! values above the ASCII range and, on 16-bit `wchar_t`, lone surrogates.
//! Case mapping is the C library's in the current locale on both sides.
//! The dstr conversions get arbitrary UTF-8 and arbitrary bytes, and are
//! compared on `array` NULL-ness, bytes, `len` and `capacity`; wide results
//! element for element. Pointer results are compared as offsets.
//!
//! No intentional differences.
//!
//! Mutation check (done for #1855): testing `n` before the decrement in
//! `compare` (one character more than C compares) fails
//! `compares_match_c`, and dropping `'\r'` from `is_padding` fails
//! `wcsdepad_matches_c`, along with the Tier 1 cases. Reverted.

use core::ffi::{c_char, c_void};
use core::ptr;

use obs_c_oracle::dstr::OracleDstr;
use obs_c_oracle::dstr_libc as c;
use obs_util::dstr_libc::WChar;
use obs_util::ffi::darray::bfree;
use obs_util::ffi::dstr::dstr;
use obs_util::ffi::dstr_libc as r;
use proptest::collection::vec;
use proptest::prelude::*;

fn wc(v: u32) -> WChar {
    v as WChar
}

fn wide_char() -> impl Strategy<Value = WChar> {
    prop_oneof![
        (0x20u32..0x7f).prop_map(wc),
        prop::sample::select(vec![0x20u32, 0x09, 0x0a, 0x0d, 0x0b, 0x0c]).prop_map(wc),
        prop::sample::select(vec![
            0xe9u32, 0xc9, 0xdf, 0xff, 0x178, 0x3a3, 0x3c3, 0x3c2, 0x130, 0x131, 0x20ac
        ])
        .prop_map(wc),
        (1u32..0x11_0000).prop_map(wc),
    ]
    .prop_filter("no NUL", |&c| c != 0)
}

/// A NUL-terminated wide string.
fn wide() -> impl Strategy<Value = Vec<WChar>> {
    vec(wide_char(), 0..12).prop_map(|mut v| {
        v.push(0);
        v
    })
}

/// A short NUL-terminated wide string over a small alphabet, so searches
/// and compares often meet equal prefixes.
fn small_wide() -> impl Strategy<Value = Vec<WChar>> {
    vec(
        prop::sample::select(vec![0x61u32, 0x41, 0x62, 0x42, 0x20, 0xe9, 0xc9]).prop_map(wc),
        0..6,
    )
    .prop_map(|mut v| {
        v.push(0);
        v
    })
}

fn opt_ptr(v: &Option<Vec<WChar>>) -> *const WChar {
    v.as_ref().map_or(ptr::null(), |v| v.as_ptr())
}

fn new_r() -> dstr {
    dstr {
        array: ptr::null_mut(),
        len: 0,
        capacity: 0,
    }
}

fn new_c() -> OracleDstr {
    OracleDstr {
        array: ptr::null_mut(),
        len: 0,
        capacity: 0,
    }
}

/// A dstr holding `bytes` (NUL-terminated after them), built the way
/// `dstr_ncopy` would: capacity `len + 1`, or freed when empty.
fn fill(bytes: &[u8]) -> (*mut c_char, usize, usize) {
    if bytes.is_empty() {
        return (ptr::null_mut(), 0, 0);
    }
    // SAFETY: bmalloc returns `len + 1` writable bytes.
    unsafe {
        let p = obs_util::ffi::darray::bmalloc(bytes.len() + 1).cast::<u8>();
        ptr::copy_nonoverlapping(bytes.as_ptr(), p, bytes.len());
        *p.add(bytes.len()) = 0;
        (p.cast(), bytes.len(), bytes.len() + 1)
    }
}

/// `(array is NULL, bytes, len, capacity)`, then frees the array.
///
/// # Safety
///
/// `array` is null or a bmalloc'ed buffer of at least `len` bytes.
unsafe fn take(array: *mut c_char, len: usize, capacity: usize) -> (bool, Vec<u8>, usize, usize) {
    let bytes = if array.is_null() {
        Vec::new()
    } else {
        // SAFETY: forwarded to the caller.
        unsafe { core::slice::from_raw_parts(array.cast::<u8>(), len) }.to_vec()
    };
    // SAFETY: forwarded to the caller.
    unsafe { bfree(array.cast()) };
    (array.is_null(), bytes, len, capacity)
}

/// The NUL-terminated wide string at `p` (terminator included), freed.
///
/// # Safety
///
/// `p` is null or a bmalloc'ed NUL-terminated wide string.
unsafe fn take_wide(p: *mut WChar) -> Option<Vec<WChar>> {
    if p.is_null() {
        return None;
    }
    let mut out = Vec::new();
    let mut i = 0;
    loop {
        // SAFETY: NUL-terminated (caller).
        let ch = unsafe { *p.add(i) };
        out.push(ch);
        if ch == 0 {
            break;
        }
        i += 1;
    }
    // SAFETY: from bmalloc (caller).
    unsafe { bfree(p.cast()) };
    Some(out)
}

/// The C string at `p` (terminator excluded), freed.
///
/// # Safety
///
/// `p` is null or a bmalloc'ed NUL-terminated string.
unsafe fn take_c(p: *mut c_char) -> Option<Vec<u8>> {
    if p.is_null() {
        return None;
    }
    // SAFETY: NUL-terminated (caller).
    let v = unsafe { core::ffi::CStr::from_ptr(p) }.to_bytes().to_vec();
    // SAFETY: from bmalloc (caller).
    unsafe { bfree(p.cast()) };
    Some(v)
}

fn text() -> impl Strategy<Value = Vec<u8>> {
    prop_oneof![
        any::<String>().prop_map(String::into_bytes),
        "[a-zA-Z \u{e9}\u{c9}\u{df}\u{3a3}\u{3c3}\u{130}\u{1f600}]{0,10}"
            .prop_map(String::into_bytes),
        vec(1u8..=255, 0..12),
    ]
}

proptest! {
    #![proptest_config(ProptestConfig { cases: 512, ..ProptestConfig::default() })]

    #[test]
    fn compares_match_c(
        a in prop::option::weighted(0.9, prop_oneof![wide(), small_wide()]),
        b in prop::option::weighted(0.9, prop_oneof![wide(), small_wide()]),
        n in 0usize..16,
    ) {
        let (pa, pb) = (opt_ptr(&a), opt_ptr(&b));
        let (ca, cb) = (pa.cast::<c_void>(), pb.cast::<c_void>());
        // SAFETY: null or NUL-terminated wide strings.
        unsafe {
            prop_assert_eq!(r::wstrcmpi(pa, pb), c::oracle_wstrcmpi(ca, cb));
            prop_assert_eq!(r::wstrcmp_n(pa, pb, n), c::oracle_wstrcmp_n(ca, cb, n));
            prop_assert_eq!(r::wstrcmpi_n(pa, pb, n), c::oracle_wstrcmpi_n(ca, cb, n));
            let rs = r::wstrstri(pa, pb);
            let cs = c::oracle_wstrstri(ca, cb).cast::<WChar>();
            let off = |p: *mut WChar| (!p.is_null()).then(|| p.offset_from(pa));
            prop_assert_eq!(off(rs), off(cs));
        }
    }

    #[test]
    fn wcsdepad_matches_c(s in prop::option::weighted(0.95, wide())) {
        let mut rs = s.clone();
        let mut cs = s;
        let rp = rs.as_mut().map_or(ptr::null_mut(), |v| v.as_mut_ptr());
        let cp = cs.as_mut().map_or(ptr::null_mut(), |v| v.as_mut_ptr());
        // SAFETY: null or writable NUL-terminated wide strings.
        unsafe {
            prop_assert_eq!(r::wcsdepad(rp), rp);
            prop_assert_eq!(c::oracle_wcsdepad(cp.cast()).cast::<WChar>(), cp);
        }
        prop_assert_eq!(rs, cs);
    }

    #[test]
    fn dstr_conversions_match_c(bytes in text(), w in wide(), old in text()) {
        // SAFETY: valid dstrs with bmalloc'ed arrays; NUL-terminated
        // strings; every result is freed once.
        unsafe {
            // dstr_to_wcs / dstr_to_mbs
            let (array, len, capacity) = fill(&bytes);
            let rd = dstr { array, len, capacity };
            let cd = OracleDstr { array, len, capacity };
            prop_assert_eq!(take_wide(r::dstr_to_wcs(&rd)),
                take_wide(c::oracle_dstr_to_wcs(&cd).cast()));
            prop_assert_eq!(take_c(r::dstr_to_mbs(&rd)), take_c(c::oracle_dstr_to_mbs(&cd)));
            bfree(array.cast());

            // dstr_from_wcs over an existing dstr
            let (array, len, capacity) = fill(&old);
            let mut rd = dstr { array, len, capacity };
            let (array, len, capacity) = fill(&old);
            let mut cd = OracleDstr { array, len, capacity };
            r::dstr_from_wcs(&mut rd, w.as_ptr());
            c::oracle_dstr_from_wcs(&mut cd, w.as_ptr().cast());
            prop_assert_eq!(take(rd.array, rd.len, rd.capacity),
                take(cd.array, cd.len, cd.capacity));

            // dstr_from_mbs
            let mut nul_free: Vec<u8> = bytes.iter().copied().filter(|&b| b != 0).collect();
            nul_free.push(0);
            let (array, len, capacity) = fill(&old);
            let mut rd = dstr { array, len, capacity };
            let (array, len, capacity) = fill(&old);
            let mut cd = OracleDstr { array, len, capacity };
            r::dstr_from_mbs(&mut rd, nul_free.as_ptr().cast());
            c::oracle_dstr_from_mbs(&mut cd, nul_free.as_ptr().cast());
            prop_assert_eq!(take(rd.array, rd.len, rd.capacity),
                take(cd.array, cd.len, cd.capacity));
        }
    }

    #[test]
    fn dstr_case_matches_c(bytes in text(), upper in any::<bool>(), nul_first in any::<bool>()) {
        let (array, len, capacity) = fill(&bytes);
        let mut rd = dstr { array, len, capacity };
        let (array, len, capacity) = fill(&bytes);
        let mut cd = OracleDstr { array, len, capacity };
        // SAFETY: valid dstrs with bmalloc'ed arrays, freed once each.
        unsafe {
            if nul_first && !rd.array.is_null() {
                *rd.array = 0;
                *cd.array = 0;
            }
            if upper {
                r::dstr_to_upper(&mut rd);
                c::oracle_dstr_to_upper(&mut cd);
            } else {
                r::dstr_to_lower(&mut rd);
                c::oracle_dstr_to_lower(&mut cd);
            }
            prop_assert_eq!(take(rd.array, rd.len, rd.capacity),
                take(cd.array, cd.len, cd.capacity));
        }
    }
}

#[test]
fn null_conversions_match_c() {
    let (mut rd, mut cd) = (new_r(), new_c());
    // SAFETY: empty dstrs and a NULL source.
    unsafe {
        r::dstr_from_mbs(&mut rd, ptr::null());
        c::oracle_dstr_from_mbs(&mut cd, ptr::null());
        assert_eq!((rd.array.is_null(), rd.len, rd.capacity), (true, 0, 0));
        assert_eq!((cd.array.is_null(), cd.len, cd.capacity), (true, 0, 0));
        assert!(r::dstr_to_wcs(&rd).is_null());
        assert!(c::oracle_dstr_to_wcs(&cd).is_null());
    }
}
