//! Tier 1 mirrors the exported-function assertions in test_cf_lexer.c.
use obs_util::cf_parser::{Cursor, diagnostic, pass_pair};
use obs_util::cf_tokenizer::{CfLexed, CfTokenType, cf_lex};

struct Tokens {
    lex: CfLexed,
    index: usize,
}
impl Tokens {
    fn new(text: &[u8]) -> Self {
        Self {
            lex: cf_lex(text),
            index: 0,
        }
    }
}
impl Cursor for Tokens {
    fn token(&self) -> (i32, u8) {
        let t = &self.lex.tokens[self.index];
        (
            t.kind as i32,
            self.lex.reformatted.get(t.str_start).copied().unwrap_or(0),
        )
    }
    fn advance(&mut self) {
        self.index += 1;
    }
}

#[test]
fn parser_pass_pair_test() {
    let mut t = Tokens::new(b"a { b ( c ) } d");
    assert_eq!(t.token(), (CfTokenType::Name as i32, b'a'));
    t.index = 2;
    assert_eq!(t.token(), (CfTokenType::Other as i32, b'{'));
    assert!(pass_pair(&mut t, b'{', b'}'));
    assert_eq!(t.token().0, CfTokenType::SpaceTab as i32);
    t.advance();
    assert_eq!(t.token().1, b'd');
    t.advance();
    assert_eq!(t.token().0, CfTokenType::None as i32);

    let mut t = Tokens::new(b"{ x { y } z } w");
    assert!(pass_pair(&mut t, b'{', b'}'));
    t.advance();
    assert_eq!(t.token().1, b'w');
    let mut t = Tokens::new(b"{ x");
    assert!(!pass_pair(&mut t, b'{', b'}'));
    assert_eq!(t.token().0, CfTokenType::None as i32);
    let mut t = Tokens::new(b"q");
    assert!(pass_pair(&mut t, b'{', b'}'));
    assert_eq!(t.index, 0);
}

#[test]
fn parser_adderror_test() {
    assert_eq!(
        diagnostic(b"a\n  b", 4, b"Expected '$1'", [Some(b"x"), None, None]),
        (2, 3, b"Expected 'x'".to_vec())
    );
    assert_eq!(
        diagnostic(b"a\n  b", 4, b"Syntax error", [None; 3]),
        (2, 3, b"Syntax error".to_vec())
    );
    assert_eq!(
        diagnostic(b"a\n  b", 4, b"just a warning", [None; 3]),
        (2, 3, b"just a warning".to_vec())
    );
}

#[test]
fn parser_edges() {
    for (text, ok, index) in [
        (b"".as_slice(), false, 0),
        (b"()", true, 0),
        (b"{}", true, 2),
        (b"{{", false, 2),
    ] {
        let mut t = Tokens::new(text);
        assert_eq!(pass_pair(&mut t, b'{', b'}'), ok);
        assert_eq!(t.index, index);
    }
    assert_eq!(
        diagnostic(
            b"",
            0,
            b"$1/$2/$3/$4",
            [Some(b"$2"), Some(b"B"), Some(b"C")]
        )
        .2,
        b"B/B/C/$4"
    );
    assert_eq!(
        diagnostic(b"", 0, b"$1/$2/$3", [None, Some(b""), Some(b"C")]).2,
        b"$1//C"
    );
}

#[test]
fn empty_formatted_diagnostic_preserves_null() {
    use obs_util::cf_parser::nullable_diagnostic;
    assert_eq!(
        nullable_diagnostic(b"", 0, Some(b""), [None; 3]).2,
        Some(vec![])
    );
    assert_eq!(
        nullable_diagnostic(b"", 0, Some(b""), [None, None, Some(b"")]).2,
        None
    );
    assert_eq!(
        nullable_diagnostic(b"", 0, Some(b"$1"), [Some(b""), None, None]).2,
        Some(vec![])
    );
}
