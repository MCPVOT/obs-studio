//! Safe core of the two exported cf-parser functions.

/// Token access shared by the C adapter and safe test cursors.
pub trait Cursor {
    /// C token kind and first byte (zero for the terminator).
    fn token(&self) -> (i32, u8);
    /// Move to the next token in the terminated stream.
    fn advance(&mut self);
}

/// Skip a delimiter pair, preserving the C cursor postcondition.
pub fn pass_pair(cursor: &mut impl Cursor, open: u8, close: u8) -> bool {
    let (kind, byte) = cursor.token();
    if kind != 6 || byte != open {
        return kind != 0;
    }
    cursor.advance();
    while cursor.token().0 != 0 {
        let byte = cursor.token().1;
        if byte == open {
            if !pass_pair(cursor, open, close) {
                break;
            }
            continue;
        } else if byte == close {
            cursor.advance();
            return true;
        }
        cursor.advance();
    }
    false
}

/// Format a parser diagnostic using the original sequential substitutions.
pub fn diagnostic(
    text: &[u8],
    position: usize,
    message: &[u8],
    values: [Option<&[u8]>; 3],
) -> (u32, u32, Vec<u8>) {
    let (row, column, bytes) = nullable_diagnostic(text, position, Some(message), values);
    (row, column, bytes.unwrap_or_default())
}

/// Preserve a null plain diagnostic, which error_data_add stores as null.
pub fn nullable_diagnostic(
    text: &[u8],
    position: usize,
    message: Option<&[u8]>,
    values: [Option<&[u8]>; 3],
) -> (u32, u32, Option<Vec<u8>>) {
    let (row, column) = crate::lexer::str_offset_in(text, position);
    let bytes = if values.iter().all(Option::is_none) {
        message.map(<[u8]>::to_vec)
    } else {
        let mut formatted = crate::dstr::Dstr::new();
        formatted.safe_printf(message, [values[0], values[1], values[2], None]);
        (formatted.capacity() != 0).then(|| formatted.as_bytes().to_vec())
    };
    (row, column, bytes)
}
