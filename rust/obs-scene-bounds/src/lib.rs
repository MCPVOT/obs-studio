//! Rust port of `libobs/obs-scene-bounds.c.h`.
//!
//! [`fit`] is the safe core. The C ABI shim lives in [`ffi`].

#![deny(unsafe_code)]

pub mod fit;

#[allow(unsafe_code)]
pub mod ffi;

pub use fit::{BoundsType, bounds_fit_scale};
pub use obs_graphics::vec2::Vec2;
