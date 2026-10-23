//! Low-level bindings to the original libobs C ABI.
//!
//! `bundled` builds the native library through obs-build and cc::Build. Without
//! it the declarations can be used by a parent supplying its own libobs linkage.
//!
//! Bundled builds accept `OBS_SYS_UNITY=0|1` (default `1`) and
//! `OBS_SYS_PCH=0|1` (default `0`). PCH currently requires Linux GCC or Clang.
//! Cargo tracks both options and rebuilds the native library when they change.
//!
//! Optimization and symbols follow Cargo's effective `OPT_LEVEL` and `DEBUG`.
//! `OBS_SYS_NATIVE_PROFILE=debug|release|relwithdebinfo|minsizerel` overrides both.
//! Native assertions default on at optimization level 0 and off otherwise;
//! `OBS_SYS_NATIVE_ASSERTIONS=0|1` overrides that policy independently of symbols.
//! This native policy does not discover custom Rust debug-assertion settings.
#![deny(unsafe_code)]

#[allow(unsafe_code)]
pub mod ffi;

/// Artifact and mode metadata for consumers of the bundled native build.
pub const BUNDLED_LIBRARY: Option<&str> = option_env!("OBS_SYS_LIBRARY");
pub const NATIVE_UNITY: Option<&str> = option_env!("OBS_SYS_NATIVE_UNITY");
pub const NATIVE_PCH: Option<&str> = option_env!("OBS_SYS_NATIVE_PCH");
