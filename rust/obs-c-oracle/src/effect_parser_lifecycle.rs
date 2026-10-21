//! Original-C effect-parser lifecycle fixture and layout probes.
use core::ffi::c_void;
unsafe extern "C" {
    pub fn oracle_ep_free(parser: *mut c_void);
    pub fn oracle_effect_parser_layout(index: usize) -> usize;
    pub fn oracle_effect_parser_fixture(seed: u32) -> *mut c_void;
    pub fn oracle_effect_parser_check(fixture: *mut c_void) -> u32;
}
