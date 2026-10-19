//! Rust port of `libobs/callback/calldata.c.h`, `callback/decl.c.h`
//! and `callback/proc.c.h`.
//!
//! [`calldata`], [`decl`] and [`proc`] are the safe cores. The C ABI shims live in
//! [`ffi`].

#![deny(unsafe_code)]

pub mod calldata;
pub mod decl;
pub mod proc;

#[allow(unsafe_code)]
pub mod ffi;

pub use calldata::{Calldata, FIXED_OVERFLOW_MESSAGE, StringGet};
pub use decl::{Decl, DeclParam, DeclParse, ParamType, parse_decl};
pub use proc::{AddOutcome, PreparedCall, ProcHandler};
