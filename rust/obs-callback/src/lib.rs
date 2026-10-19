//! Rust port of `libobs/callback/calldata.c.h`.
//!
//! [`calldata`] is the safe core. The C ABI shims live in [`ffi`].

#![deny(unsafe_code)]

pub mod calldata;

#[allow(unsafe_code)]
pub mod ffi;

pub use calldata::{Calldata, FIXED_OVERFLOW_MESSAGE, StringGet};
