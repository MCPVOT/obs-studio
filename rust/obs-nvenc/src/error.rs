//! Typed load errors. `Display` keeps the C plugin's log prefix
//! (`[NVENC] Test process failed: cuda_lib`) so log scrapers see the same
//! text, followed by what was tried and why.

use std::fmt;

use crate::{Candidate, CudaVersion, Library};

/// Why a driver library could not be loaded.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum LoadError {
    /// Every candidate failed to open; each with its loader error.
    NotFound {
        library: Library,
        tried: Vec<(Candidate, String)>,
    },
    /// A candidate exists but needs a newer glibc than the process has
    /// (`version 'GLIBC_2.44' not found`).
    GlibcMismatch {
        candidate: Candidate,
        required: String,
    },
    /// A required symbol is missing from the opened library.
    MissingSymbol {
        library: Library,
        symbol: &'static str,
    },
    /// The driver is older than required.
    DriverTooOld {
        found: CudaVersion,
        need: CudaVersion,
    },
}

impl fmt::Display for LoadError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let _ = f;
        todo!("LoadError::fmt")
    }
}

impl std::error::Error for LoadError {}

/// A CUDA call failed.
#[derive(Debug, Clone, PartialEq, Eq)]
#[non_exhaustive]
pub enum CudaError {
    /// The driver returned a non-zero `CUresult`.
    Call { function: &'static str, result: i32 },
    /// The optional symbol is not present in this driver.
    SymbolUnavailable(&'static str),
}

impl fmt::Display for CudaError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self {
            CudaError::Call { function, result } => {
                write!(f, "CUDA call \"{function}\" failed with {result}")
            }
            CudaError::SymbolUnavailable(s) => write!(f, "CUDA function {s} is not available"),
        }
    }
}

impl std::error::Error for CudaError {}

/// Extracts `GLIBC_x.y` from a dynamic loader message such as
/// ``/usr/lib/libcuda.so.1: version `GLIBC_2.44' not found (required by ...)``.
pub fn parse_glibc_requirement(dlerror: &str) -> Option<String> {
    let _ = dlerror;
    todo!("parse_glibc_requirement")
}
