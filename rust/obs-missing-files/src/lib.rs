//! Rust port of `libobs/obs-missing-files.c.h`.
//!
//! [`files`] is the safe core. The C ABI shims live in [`ffi`].

#![deny(unsafe_code)]

pub mod files;

#[allow(unsafe_code)]
pub mod ffi;

pub use files::{SCRIPT, SOURCE};
