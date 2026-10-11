//! Safe core for `libobs/util/pipe.c` and the POSIX half,
//! `libobs/util/pipe-posix.c`. The Windows half,
//! `libobs/util/pipe-windows.c`, is in [`windows`] and exports the same
//! `Pipe` API.
//!
//! The C ABI shim lives in [`crate::ffi::pipe`]. `Args` is platform-neutral
//! (a null-terminated argv like the C darray), and so is
//! [`windows_command_line`], the quoting of `os_process_pipe_create2` on
//! Windows.
//!
//! On Unix, spawning goes through [`std::process::Command`] rather than raw
//! `posix_spawn`, so `unsafe` stays in the shim. The observable contract
//! matches the C: pipes are created `CLOEXEC`, the environment is
//! inherited, `stderr` is always captured, streams are closed before the
//! child is reaped, and the exit status is cast through the platform
//! `char`, so 255 becomes -1 where `char` is signed.

use core::ffi::c_char;
#[cfg(unix)]
use core::ffi::c_int;
use std::ffi::CString;

/// Owned, null-terminated argument vector. Mirrors `struct os_process_args`:
/// a list of duplicated strings with a trailing NULL kept valid after every
/// mutation, like the C darray.
pub struct Args {
    args: Vec<CString>,
    /// Cached `argv`: one pointer per arg plus a trailing null, rebuilt on
    /// every mutation so [`Args::argv_ptr`] never dangles.
    ptrs: Vec<*mut c_char>,
}

/// Truncate at the first NUL, mirroring `bstrdup` (which copies with
/// `strlen` semantics and cannot represent bytes past a NUL).
fn truncated(input: &[u8]) -> &[u8] {
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

#[cfg(windows)]
pub mod windows;
#[cfg(windows)]
pub use windows::Pipe;

/// A spawned child with its stdio pipes. Mirrors `struct os_process_pipe`
/// on POSIX: `read_mode` selects `stdout` vs `stdin`, and `stderr` is always
/// captured.
#[cfg(unix)]
pub struct Pipe {
    child: std::process::Child,
    /// `Some` in read mode (`stdout`); `None` in write mode. The presence
    /// of the streams encodes the C `read_pipe` flag.
    reader: Option<std::process::ChildStdout>,
    writer: Option<std::process::ChildStdin>,
    err: Option<std::process::ChildStderr>,
}

#[cfg(unix)]
fn c_string(bytes: &[u8]) -> Option<CString> {
    CString::new(truncated(bytes)).ok()
}

#[cfg(unix)]
fn spawn(argv0: &CString, rest: &[&CString], read_mode: bool) -> Option<Pipe> {
    use std::os::unix::ffi::OsStrExt;
    use std::process::{Command, Stdio};

    let mut cmd = Command::new(std::ffi::OsStr::from_bytes(argv0.as_bytes()));
    for arg in rest {
        cmd.arg(std::ffi::OsStr::from_bytes(arg.as_bytes()));
    }
    // Rust sets CLOEXEC on every new fd and closes non-stdio fds in the
    // child, matching the C `fcntl` + `posix_spawn` file actions.
    if read_mode {
        cmd.stdin(Stdio::null()).stdout(Stdio::piped());
    } else {
        cmd.stdin(Stdio::piped()).stdout(Stdio::null());
    }
    // The C always captures stderr, in both modes.
    cmd.stderr(Stdio::piped());
    let mut child = cmd.spawn().ok()?;
    let (reader, writer) = if read_mode {
        (child.stdout.take(), None)
    } else {
        (None, child.stdin.take())
    };
    let err = child.stderr.take();
    Some(Pipe {
        child,
        reader,
        writer,
        err,
    })
}

#[cfg(unix)]
impl Pipe {
    /// Only a leading `r` selects read mode; anything else (including empty)
    /// is write mode, exactly like `*type == 'r'`.
    fn read_mode(type_: &[u8]) -> bool {
        type_.first().copied() == Some(b'r')
    }

    /// Mirrors `os_process_pipe_create`: runs `/bin/sh -c cmd_line`.
    /// A null command is `None`.
    pub fn create(cmd_line: Option<&[u8]>, type_: &[u8]) -> Option<Self> {
        let cmd = c_string(cmd_line?)?;
        let sh = c_string(b"/bin/sh")?;
        let dash_c = c_string(b"-c")?;
        spawn(&sh, &[&dash_c, &cmd], Self::read_mode(type_))
    }

    /// Mirrors `os_process_pipe_create2`: spawns `args` directly, with
    /// `argv[0]` as the program. A null `args` is `None`; C would
    /// dereference it.
    pub fn create2(args: Option<&Args>, type_: &[u8]) -> Option<Self> {
        let args = args?;
        if args.args.is_empty() {
            return None;
        }
        spawn(
            &args.args[0],
            &args.args[1..].iter().collect::<Vec<_>>(),
            Self::read_mode(type_),
        )
    }

    /// Closes the streams, reaps the child, and returns its exit status with
    /// the C `(int)(char)` cast applied, mirroring
    /// `os_process_pipe_destroy`. A signaled child has no status code; C
    /// falls through to returning the `waitpid` pid, so this returns the
    /// child's pid too.
    pub fn destroy(self) -> c_int {
        use std::os::unix::process::ExitStatusExt;
        let Pipe {
            mut child,
            reader,
            writer,
            err,
        } = self;
        let pid = child.id();
        // fclose order in C is `file` then `err_file`, both before waitpid.
        drop(reader);
        drop(writer);
        drop(err);
        match child.wait() {
            Ok(status) => match status.code() {
                Some(code) => code as c_char as c_int,
                None => {
                    debug_assert!(status.signal().is_some());
                    pid as c_int
                }
            },
            Err(_) => -1,
        }
    }

    /// The child's pid, for tests that signal it. Mirrors the `pid` field
    /// of `struct os_process_pipe`.
    pub fn pid(&self) -> u32 {
        self.child.id()
    }

    /// One read from the child's stdout, mirroring the single `fread` in
    /// `os_process_pipe_read` (short reads are returned, not filled).
    /// Zero for a write-mode pipe.
    pub fn read(&mut self, buf: &mut [u8]) -> usize {
        use std::io::Read;
        let Some(reader) = self.reader.as_mut() else {
            return 0;
        };
        loop {
            match reader.read(buf) {
                Ok(n) => return n,
                Err(err) if err.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(_) => return 0,
            }
        }
    }

    /// One read from the child's stderr, mirroring
    /// `os_process_pipe_read_err`. Unlike [`Pipe::read`] this works on a
    /// write-mode pipe too.
    pub fn read_err(&mut self, buf: &mut [u8]) -> usize {
        use std::io::Read;
        let Some(err) = self.err.as_mut() else {
            return 0;
        };
        loop {
            match err.read(buf) {
                Ok(n) => return n,
                Err(err) if err.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(_) => return 0,
            }
        }
    }

    /// Write until `buf` is consumed or the pipe errors, mirroring the
    /// `fwrite` loop in `os_process_pipe_write`. Zero for a read-mode pipe.
    /// A write to a closed reader surfaces as a short count here; in a
    /// C-main process (libobs) both sides die on SIGPIPE identically, since
    /// signal disposition is a process property (Rust `std` only ignores
    /// SIGPIPE in processes whose `main` is Rust). Neither side touches the
    /// child's signal mask: `posix_spawn` with no attributes and
    /// `Command` both inherit it.
    pub fn write(&mut self, buf: &[u8]) -> usize {
        use std::io::Write;
        let Some(writer) = self.writer.as_mut() else {
            return 0;
        };
        let mut done = 0;
        while done < buf.len() {
            match writer.write(&buf[done..]) {
                Ok(0) => break,
                Ok(n) => done += n,
                Err(err) if err.kind() == std::io::ErrorKind::Interrupted => continue,
                Err(_) => break,
            }
        }
        done
    }
}
