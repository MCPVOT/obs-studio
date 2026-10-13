//! Walks the candidates and resolves the CUDA driver function table.

use std::collections::HashMap;
use std::ffi::c_void;
use std::fmt;
use std::ptr::NonNull;

use crate::{Candidate, CudaError, Host, Library, LoadError, SystemEnv, SystemLoader, candidates};

/// A resolved symbol address. Only the platform loader turns it back into a
/// callable function.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Symbol(NonNull<c_void>);

impl Symbol {
    /// Wraps a non-null address.
    pub fn from_ptr(ptr: *mut c_void) -> Option<Symbol> {
        NonNull::new(ptr).map(Symbol)
    }

    /// The raw address.
    pub fn as_ptr(self) -> *mut c_void {
        self.0.as_ptr()
    }
}

/// An opened library.
pub trait DynLib {
    /// Resolves `name`, or `None` if the library does not export it.
    fn symbol(&self, name: &str) -> Option<Symbol>;
    /// Calls `cuDriverGetVersion` through `sym`; returns `(CUresult, version)`.
    fn call_driver_get_version(&self, sym: Symbol) -> (i32, i32);
}

/// Opens a candidate; the error string is the loader's message (`dlerror`).
pub trait DynLoader {
    fn open(&self, c: &Candidate) -> Result<Box<dyn DynLib>, String>;
}

/// A CUDA driver API version (`cuDriverGetVersion`: `1000 * major + 10 * minor`).
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash)]
pub struct CudaVersion(pub i32);

impl CudaVersion {
    pub fn major(self) -> i32 {
        self.0 / 1000
    }
    pub fn minor(self) -> i32 {
        (self.0 % 1000) / 10
    }
}

impl fmt::Display for CudaVersion {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}.{}", self.major(), self.minor())
    }
}

/// Symbols resolved when present but not required (not in `cuda_functions[]`).
pub const OPTIONAL_CUDA_SYMBOLS: &[&str] = &["cuDriverGetVersion"];

/// The names in `cuda_functions[]` (`plugins/obs-nvenc/cuda-helpers.c`), in
/// order, for `host`; the GL interop entries are absent on Windows.
pub fn required_cuda_symbols(host: Host) -> Vec<&'static str> {
    let _ = host;
    todo!("required_cuda_symbols")
}

/// Opens the first candidate that loads. When none does, a `GLIBC_x.y`
/// mismatch is reported in preference to the plain `NotFound` list.
pub fn open_library(
    library: Library,
    cands: &[Candidate],
    dl: &dyn DynLoader,
) -> Result<(Candidate, Box<dyn DynLib>), LoadError> {
    let _ = (library, cands, dl);
    todo!("open_library")
}

/// The loaded CUDA driver and its resolved function table.
pub struct CudaDriver {
    candidate: Candidate,
    symbols: HashMap<&'static str, Symbol>,
    lib: Box<dyn DynLib>,
}

impl fmt::Debug for CudaDriver {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("CudaDriver")
            .field("candidate", &self.candidate)
            .field("symbols", &self.symbols.len())
            .finish()
    }
}

impl CudaDriver {
    /// Loads the driver from the current host's candidates.
    pub fn load() -> Result<Self, LoadError> {
        Self::load_from(&candidates(Library::CudaDriver, &SystemEnv), &SystemLoader)
    }

    /// Loads the driver from `cands` through `dl`, for the current host's
    /// symbol table.
    pub fn load_from(cands: &[Candidate], dl: &dyn DynLoader) -> Result<Self, LoadError> {
        Self::load_from_host(cands, dl, Host::current())
    }

    /// [`CudaDriver::load_from`] with an explicit host symbol table.
    pub fn load_from_host(
        cands: &[Candidate],
        dl: &dyn DynLoader,
        host: Host,
    ) -> Result<Self, LoadError> {
        let _ = (cands, dl, host);
        todo!("CudaDriver::load_from_host")
    }

    /// The candidate that loaded.
    pub fn candidate(&self) -> &Candidate {
        &self.candidate
    }

    /// The resolved address of `name` (required or optional symbol).
    pub fn symbol(&self, name: &str) -> Option<Symbol> {
        self.symbols.get(name).copied()
    }

    /// `cuDriverGetVersion`.
    pub fn driver_version(&self) -> Result<CudaVersion, CudaError> {
        todo!("driver_version {:p}", &self.lib)
    }

    /// Fails with [`LoadError::DriverTooOld`] when the driver is older than `need`.
    pub fn require_version(&self, need: CudaVersion) -> Result<CudaVersion, LoadError> {
        let _ = need;
        todo!("require_version")
    }
}
