//! Which file names to try, in which order. Pure data: every host's list is
//! available on every host (so Tier 1 checks all of them everywhere), and
//! `platform` only picks which [`Host`] is current.

use std::ffi::OsString;

/// A driver library OBS opens at run time.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Library {
    /// The CUDA driver API (`libcuda.so.1` / `nvcuda.dll`).
    CudaDriver,
    /// The NVENC API (`libnvidia-encode.so.1` / `nvEncodeAPI64.dll`).
    NvEncode,
    /// NVML (`libnvidia-ml.so.1` / `nvml.dll`).
    Nvml,
}

impl Library {
    /// The environment variable that overrides the search for this library.
    pub fn env_override(self) -> &'static str {
        match self {
            Library::CudaDriver => "OBS_CUDA_LIBRARY",
            Library::NvEncode => "OBS_NVENC_LIBRARY",
            Library::Nvml => "OBS_NVML_LIBRARY",
        }
    }

    /// The tag the C plugin's test process reports when this library fails
    /// to load (`[NVENC] Test process failed: <tag>`).
    pub fn c_error_tag(self) -> &'static str {
        match self {
            Library::CudaDriver => "cuda_lib",
            Library::NvEncode => "nvenc_lib",
            Library::Nvml => "nvml_lib",
        }
    }
}

/// Where a candidate came from, for diagnostics.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum CandidateSource {
    /// `OBS_*_LIBRARY` set by the user.
    EnvOverride,
    /// The versioned soname / DLL name the C plugin opens.
    SonameDefault,
    /// The unversioned development symlink (last resort by name).
    SonameUnversioned,
    /// An absolute path in a distribution's driver directory.
    DistroPath,
    /// The WSL driver directory.
    WslPath,
}

/// One name or path to hand to the loader.
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub struct Candidate {
    pub name: OsString,
    pub source: CandidateSource,
}

impl Candidate {
    fn new(name: impl Into<OsString>, source: CandidateSource) -> Self {
        Candidate {
            name: name.into(),
            source,
        }
    }
}

/// The host family whose candidate list applies.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Host {
    /// Linux and other non-Apple Unix.
    Linux,
    /// Windows; `arm64` swaps the NVENC DLL order like `nvenc-helpers.c`.
    Windows { arm64: bool },
    /// macOS: no NVIDIA drivers, so no candidates.
    MacOs,
}

impl Host {
    /// The host this binary runs on.
    pub fn current() -> Host {
        crate::platform::HOST
    }
}

/// Read access to the environment (a seam so tests need not mutate it).
pub trait Env {
    fn var(&self, key: &str) -> Option<OsString>;
}

/// The process environment.
#[derive(Debug, Default, Clone, Copy)]
pub struct SystemEnv;

impl Env for SystemEnv {
    fn var(&self, key: &str) -> Option<OsString> {
        std::env::var_os(key)
    }
}

/// The built-in search list for `lib` on `host`, without any override.
pub fn default_candidates(lib: Library, host: Host) -> Vec<Candidate> {
    let _ = (
        lib,
        host,
        Candidate::new("", CandidateSource::SonameDefault),
    );
    todo!("default_candidates")
}

/// The full search list for `lib` on `host`: the env override (if set and
/// non-empty) first, then [`default_candidates`].
pub fn candidates_for(lib: Library, host: Host, env: &dyn Env) -> Vec<Candidate> {
    let _ = (lib, host, env);
    todo!("candidates_for")
}

/// [`candidates_for`] on the current host.
pub fn candidates(lib: Library, env: &dyn Env) -> Vec<Candidate> {
    candidates_for(lib, Host::current(), env)
}
