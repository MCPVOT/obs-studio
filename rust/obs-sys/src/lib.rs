//! Low-level bindings to the original libobs C ABI.
//!
//! `bundled` builds the native library through obs-build and cc::Build. Without
//! it the declarations can be used by a parent supplying its own libobs linkage.
//!
//! Bundled builds accept `OBS_SYS_UNITY=0|1` (default `1`) and
//! `OBS_SYS_PCH=0|1` (default `0`). PCH currently requires Linux GCC or Clang.
//! Cargo tracks both options and rebuilds the native library when they change.
#![deny(unsafe_code)]

#[allow(unsafe_code)]
pub mod ffi;
