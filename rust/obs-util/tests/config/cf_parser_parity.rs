//! Tier 2 layouts and Tier 3 original-C parity. No intentional differences.
//! Inputs are valid terminated lexer streams and source positions. Invalid
//! pointers, missing terminators and malformed synthetic tokens are excluded:
//! they violate the C API's contracts (and may loop forever in C).
use crate::common;
use core::ffi::{CStr, c_char, c_int, c_void};
use core::mem::{align_of, offset_of, size_of};
use obs_util::ffi::cf_parser::{self as rs, cf_parser, cf_preprocessor};
use obs_util::ffi::cf_tokenizer::{cf_lexer_free, cf_lexer_lex, cf_token};
use obs_util::ffi::darray::{bfree, darray_free};
use obs_util::ffi::lexer::{error_data_buildstring, error_item};
use proptest::prelude::*;
use std::ffi::CString;

unsafe extern "C" {
    fn oracle_cf_parser_pass_pair(p: *mut c_void, open: c_char, close: c_char) -> bool;
    fn oracle_cf_parser_adderror(
        p: *mut c_void,
        msg: *const c_char,
        level: c_int,
        a: *const c_char,
        b: *const c_char,
        c: *const c_char,
    );
    fn oracle_cf_parser_layout(index: usize) -> usize;
}

#[test]
fn cf_parser_layout() {
    let fields = [
        size_of::<cf_parser>(),
        align_of::<cf_parser>(),
        offset_of!(cf_parser, lex),
        offset_of!(cf_parser, pp),
        offset_of!(cf_parser, error_list),
        offset_of!(cf_parser, cur_token),
        size_of::<cf_preprocessor>(),
        align_of::<cf_preprocessor>(),
        offset_of!(cf_preprocessor, lex),
        offset_of!(cf_preprocessor, ed),
        offset_of!(cf_preprocessor, defines),
        offset_of!(cf_preprocessor, sys_include_dirs),
        offset_of!(cf_preprocessor, dependencies),
        offset_of!(cf_preprocessor, tokens),
        offset_of!(cf_preprocessor, ignore_state),
    ];
    for (i, expected) in fields.into_iter().enumerate() {
        // SAFETY: index is within the oracle's constant layout array.
        assert_eq!(expected, unsafe { oracle_cf_parser_layout(i) }, "field {i}");
    }
}

type Diagnostic = (Option<Vec<u8>>, Vec<u8>, u32, u32, i32);

struct Parser(Box<cf_parser>);
impl Parser {
    fn new(text: &str) -> Self {
        // SAFETY: all fields permit zero (pointers, scalar fields and darrays).
        let mut p: Box<cf_parser> = Box::new(unsafe { core::mem::zeroed() });
        let text = CString::new(if text.is_empty() { " " } else { text }).unwrap();
        // SAFETY: the boxed lexer stays at this address until freed; strings
        // are copied by the lexer and the filename is static.
        unsafe {
            assert!(cf_lexer_lex(&raw mut p.lex, text.as_ptr(), c"p.c".as_ptr()));
            p.cur_token = p.lex.tokens.array.cast();
        }
        Self(p)
    }
    fn at(&mut self, index: usize) {
        assert!(index < self.0.lex.tokens.num);
        // SAFETY: the bounds check includes the terminating token.
        self.0.cur_token = unsafe { self.0.lex.tokens.array.cast::<cf_token>().add(index) };
    }
    fn index(&self) -> usize {
        // SAFETY: both pointers are in the same token allocation.
        unsafe { self.0.cur_token.offset_from(self.0.lex.tokens.array.cast()) as usize }
    }
    fn errors(&self) -> Vec<Diagnostic> {
        // SAFETY: the initialized error darray holds `num` live entries.
        unsafe {
            (0..self.0.error_list.errors.num)
                .map(|i| {
                    let e = &*self.0.error_list.errors.array.cast::<error_item>().add(i);
                    (
                        if e.error.is_null() {
                            None
                        } else {
                            Some(CStr::from_ptr(e.error).to_bytes().to_vec())
                        },
                        CStr::from_ptr(e.file).to_bytes().to_vec(),
                        e.row,
                        e.column,
                        e.level,
                    )
                })
                .collect()
        }
    }
}
impl Drop for Parser {
    fn drop(&mut self) {
        // SAFETY: we exclusively own these initialized arrays and strings;
        // file names belong to the lexer and outlive error cleanup.
        unsafe {
            for i in 0..self.0.error_list.errors.num {
                bfree(
                    (*self.0.error_list.errors.array.cast::<error_item>().add(i))
                        .error
                        .cast(),
                );
            }
            darray_free(&raw mut self.0.error_list.errors);
            cf_lexer_free(&raw mut self.0.lex);
        }
    }
}
fn pair(text: &str, index: usize, open: u8, close: u8) {
    let mut a = Parser::new(text);
    let mut b = Parser::new(text);
    let index = index % a.0.lex.tokens.num;
    a.at(index);
    b.at(index);
    // SAFETY: both fixtures own valid terminated lexer streams.
    unsafe {
        assert_eq!(
            rs::cf_pass_pair(&mut *a.0, open as c_char, close as c_char),
            oracle_cf_parser_pass_pair((&raw mut *b.0).cast(), open as c_char, close as c_char)
        );
    }
    assert_eq!(a.index(), b.index());
    assert_eq!(a.errors(), b.errors());
}
fn add(a: &mut Parser, b: &mut Parser, msg: Option<&CStr>, vals: [Option<&CStr>; 3], level: i32) {
    let v = vals.map(|v| v.map_or(core::ptr::null(), CStr::as_ptr));
    // SAFETY: fixtures and C strings are live; both error lists initialized.
    unsafe {
        rs::cf_adderror(
            &mut *a.0,
            msg.map_or(core::ptr::null(), CStr::as_ptr),
            level,
            v[0],
            v[1],
            v[2],
        );
        oracle_cf_parser_adderror(
            (&raw mut *b.0).cast(),
            msg.map_or(core::ptr::null(), CStr::as_ptr),
            level,
            v[0],
            v[1],
            v[2],
        );
    }
    assert_eq!(a.errors(), b.errors());
}

#[test]
fn parser_c_characterization_boundary() {
    if !common::isolated(concat!(
        module_path!(),
        "::parser_c_characterization_boundary"
    )) {
        return;
    }
    // SAFETY: reading the process allocation counter in an isolated child.
    let before = unsafe { obs_util::ffi::bmem::bnum_allocs() };
    for (text, index) in [
        ("a { b ( c ) } d", 2),
        ("{ x { y } z } w", 0),
        ("{ x", 0),
        ("q", 0),
        ("", 0),
        ("{{", 0),
        ("{}", 0),
    ] {
        pair(text, index, b'{', b'}');
    }
    {
        let mut a = Parser::new("a\n  b");
        let mut b = Parser::new("a\n  b");
        let index = a.0.lex.tokens.num - 2;
        a.at(index);
        b.at(index);
        add(
            &mut a,
            &mut b,
            Some(c"Expected '$1'"),
            [Some(c"x"), None, None],
            0,
        );
        add(&mut a, &mut b, Some(c"Syntax error"), [None; 3], 0);
        add(&mut a, &mut b, Some(c"just a warning"), [None; 3], 1);
        let errors = a.errors();
        assert_eq!(errors.len(), 3);
        assert_eq!(errors.iter().filter(|e| e.4 == 0).count(), 2);
        assert_eq!(errors.iter().filter(|e| e.4 == 1).count(), 1);
        assert_eq!(
            errors[0],
            (Some(b"Expected 'x'".to_vec()), b"p.c".to_vec(), 2, 3, 0)
        );
        // SAFETY: initialized errors; returned string is bmalloc-owned.
        unsafe {
            let message = error_data_buildstring(&raw mut a.0.error_list);
            assert_eq!(
                CStr::from_ptr(message).to_bytes(),
                b"p.c (2, 3): Expected 'x'\np.c (2, 3): Syntax error\np.c (2, 3): just a warning\n"
            );
            bfree(message.cast());
        }
        add(&mut a, &mut b, None, [None; 3], 1);
        add(&mut a, &mut b, None, [None, Some(c"B"), None], 0);
        add(
            &mut a,
            &mut b,
            Some(c"$1/$2/$3/$4"),
            [Some(c"$2"), Some(c"B"), Some(c"C")],
            0,
        );
    }
    // SAFETY: all fixtures have been dropped in this isolated child.
    assert_eq!(unsafe { obs_util::ffi::bmem::bnum_allocs() }, before);
}

proptest! {
    #[test]
    fn cf_parser_pair_parity(text in "[a-z0-9{}() \t\r\n]{0,160}", index in any::<usize>(), round in any::<bool>()) {
        let (open, close) = if round { (b'(', b')') } else { (b'{', b'}') };
        pair(&text, index, open, close);
    }
    #[test]
    fn cf_parser_diagnostic_parity(text in "[a-z \t\r\n]{0,80}", index in any::<usize>(), msg in "[a-z$1234 ]{0,60}", a in proptest::option::of("[a-z$1234]{0,15}"), b in proptest::option::of("[a-z$1234]{0,15}"), c in proptest::option::of("[a-z$1234]{0,15}"), level in any::<i32>()) {
        let mut left = Parser::new(&text); let mut right = Parser::new(&text);
        let i = index % left.0.lex.tokens.num; left.at(i); right.at(i);
        let vals = [a,b,c].map(|s| s.map(|s| CString::new(s).unwrap()));
        let msg = CString::new(msg).unwrap();
        add(&mut left, &mut right, Some(&msg), vals.each_ref().map(|v| v.as_deref()), level);
    }
}
