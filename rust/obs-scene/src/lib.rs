//! Rust ports of the units extracted from `libobs/obs-scene.c`, one module
//! per file: `libobs/obs-scene-bounds.c.h`, `libobs/obs-scene-blend.c.h`,
//! and `libobs/obs-scene-coords.c.h`.
//!
//! The safe cores are [`bounds`], [`blend`], and [`coords`]. The C ABI
//! shims live in [`ffi`].

#![deny(unsafe_code)]

pub mod blend;
pub mod bounds;
pub mod coords;

#[allow(unsafe_code)]
pub mod ffi;
