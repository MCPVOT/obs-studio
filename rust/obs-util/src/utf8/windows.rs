//! Safe core for the Windows branch of `libobs/util/utf8.c`.
//!
//! RED: the API the shim and tests use, with no implementation yet.

/// `utf8_to_wchar` on Windows.
pub fn utf8_to_wchar(input: &[u8], out: Option<&mut [u16]>) -> usize {
    let _ = (input, out);
    todo!("port the Windows branch of utf8.c utf8_to_wchar")
}

/// `wchar_to_utf8` on Windows.
pub fn wchar_to_utf8(input: &[u16], out: Option<&mut [u8]>) -> usize {
    let _ = (input, out);
    todo!("port the Windows branch of utf8.c wchar_to_utf8")
}
