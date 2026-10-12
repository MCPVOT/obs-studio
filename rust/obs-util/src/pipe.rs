//! Safe core for `libobs/util/pipe.c` and the POSIX half,
//! `libobs/util/pipe-posix.c`. The Windows half,
//! `libobs/util/pipe-windows.c`, is in `crate::platform::windows::pipe`.
//! Both halves live under [`crate::platform`] and export the same [`Pipe`]
//! API; this file keeps the platform-neutral parts.
//!
//! The C ABI shim lives in [`crate::ffi::pipe`]. `Args` is platform-neutral
//! (a null-terminated argv like the C darray), and so is
//! [`windows_command_line`], the quoting of `os_process_pipe_create2` on
//! Windows.
//!
//! On Unix (`crate::platform::unix::pipe`), spawning goes through [`std::process::Command`] rather than raw
//! `posix_spawn`, so `unsafe` stays in the shim. The observable contract
//! matches the C: pipes are created `CLOEXEC`, the environment is
//! inherited, `stderr` is always captured, streams are closed before the
//! child is reaped, and the exit status is cast through the platform
//! `char`, so 255 becomes -1 where `char` is signed.

use core::ffi::c_char;
use std::ffi::CString;

/// Owned, null-terminated argument vector. Mirrors `struct os_process_args`:
/// a list of duplicated strings with a trailing NULL kept valid after every
/// mutation, like the C darray.
pub struct Args {
    pub(crate) args: Vec<CString>,
    /// Cached `argv`: one pointer per arg plus a trailing null, rebuilt on
    /// every mutation so [`Args::argv_ptr`] never dangles.
    ptrs: Vec<*mut c_char>,
}

/// Truncate at the first NUL, mirroring `bstrdup` (which copies with
/// `strlen` semantics and cannot represent bytes past a NUL).
pub(crate) fn truncated(input: &[u8]) -> &[u8] {
    match input.iter().position(|&b| b == 0) {
        Some(idx) => &input[..idx],
        None => input,
    }
}

impl Args {
    fn rebuild(&mut self) {
        self.ptrs.clear();
        self.ptrs
            .extend(self.args.iter().map(|s| s.as_ptr().cast_mut()));
        self.ptrs.push(core::ptr::null_mut());
    }

    fn push(&mut self, arg: &[u8]) {
        // `CString::new` cannot fail: `truncated` removed every NUL.
        let owned = CString::new(truncated(arg)).expect("NUL-free after truncation");
        self.args.push(owned);
        self.rebuild();
    }

    /// Mirrors `os_process_args_create`: argv starts as `[executable, NULL]`.
    pub fn new(executable: &[u8]) -> Self {
        let mut fresh = Self {
            args: Vec::new(),
            ptrs: Vec::new(),
        };
        fresh.push(executable);
        fresh
    }

    /// Insert before the NULL terminator, mirroring `os_process_args_add_arg`.
    pub fn add_arg(&mut self, arg: &[u8]) {
        self.push(arg);
    }

    /// Stores already-formatted bytes: the Tier 1 spelling of the
    /// `os_process_args_add_argf` path. In production the formatting itself
    /// stays in the C wrapper in `util/pipe-variadic.c` (stable Rust cannot
    /// define a C-variadic function; it formats with `vsnprintf` and stores
    /// through the `add_arg` shim), exactly like `util/base-variadic.c`
    /// does for `blog`.
    pub fn add_formatted(&mut self, formatted: &[u8]) {
        self.push(formatted);
    }

    /// Argument count without the trailing NULL, mirroring
    /// `os_process_args_get_argc`.
    pub fn argc(&self) -> usize {
        self.args.len()
    }

    /// The `index`-th argument, or `None` past the end. The trailing NULL
    /// is not an argument; asserting it is a C-ABI property checked in
    /// `tests/pipe_parity.rs`.
    pub fn arg(&self, index: usize) -> Option<&[u8]> {
        self.args.get(index).map(|s| s.to_bytes())
    }

    /// Null-terminated argv for the spawn call, mirroring
    /// `os_process_args_get_argv`. Valid until the next mutation, like the
    /// C darray buffer. Calling this is safe; dereferencing the result is
    /// the shim's job.
    pub fn argv_ptr(&self) -> *mut *mut c_char {
        self.ptrs.as_ptr().cast_mut()
    }
}

/// The command line `os_process_pipe_create2` builds from `args` on
/// Windows, which has no argv API (`pipe-windows.c`, after Python's
/// `subprocess.list2cmdline`). Arguments are joined with spaces. An empty
/// argument, or one with a space or tab, is quoted. A quote is escaped with
/// a backslash and the backslashes before it are doubled, as are the
/// backslashes before a closing quote; other backslashes are kept.
pub fn windows_command_line(args: &Args) -> Vec<u8> {
    let mut out = Vec::new();
    for arg in &args.args {
        let arg = arg.as_bytes();
        let needs_quotes = arg.is_empty() || arg.contains(&b' ') || arg.contains(&b'\t');
        let backslashes = |out: &mut Vec<u8>, count: usize| {
            out.extend(core::iter::repeat_n(b'\\', count));
        };

        if !out.is_empty() {
            out.push(b' ');
        }
        if needs_quotes {
            out.push(b'"');
        }
        let mut pending = 0;
        for &c in arg {
            match c {
                b'\\' => pending += 1,
                b'"' => {
                    backslashes(&mut out, pending * 2);
                    out.extend_from_slice(b"\\\"");
                    pending = 0;
                }
                _ => {
                    backslashes(&mut out, pending);
                    pending = 0;
                    out.push(c);
                }
            }
        }
        backslashes(&mut out, pending);
        if needs_quotes {
            backslashes(&mut out, pending);
            out.push(b'"');
        }
    }
    out
}

pub use crate::platform::Pipe;
