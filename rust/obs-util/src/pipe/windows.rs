//! Windows half of the pipe port: `libobs/util/pipe-windows.c`.
//!
//! The C is kept call for call rather than going through
//! [`std::process::Command`], whose observable behavior differs on Windows:
//! it resolves the program itself instead of letting `CreateProcessW` parse
//! the command line (so the search order differs), it cannot set
//! `STARTF_FORCEOFFFEEDBACK`, and it limits which handles the child
//! inherits. So this module calls Win32 directly and keeps its `unsafe`
//! to the handle calls.
//!
//! Intentional difference: when creating a pipe fails half-way, C leaks the
//! handles it already made (both stderr pipe ends, or the stdio pair when
//! the second `CreatePipe` fails); this closes them. Only the process's
//! handle count can tell.

use core::ffi::c_int;
use core::ptr;

use windows_sys::Win32::Foundation::{
    CloseHandle, GetLastError, HANDLE, HANDLE_FLAG_INHERIT, SetHandleInformation,
};
use windows_sys::Win32::Globalization::{CP_UTF8, MultiByteToWideChar};
use windows_sys::Win32::Security::SECURITY_ATTRIBUTES;
use windows_sys::Win32::Storage::FileSystem::{ReadFile, WriteFile};
use windows_sys::Win32::System::Pipes::CreatePipe;
use windows_sys::Win32::System::Threading::{
    CREATE_NO_WINDOW, CreateProcessW, GetExitCodeProcess, INFINITE, PROCESS_INFORMATION,
    STARTF_FORCEOFFFEEDBACK, STARTF_USESTDHANDLES, STARTUPINFOW, WaitForSingleObject,
};

use super::{Args, windows_command_line};

/// A spawned child with its stdio pipes. Mirrors `struct os_process_pipe` in
/// `pipe-windows.c`: `handle` is the child's stdout in read mode and its
/// stdin in write mode, and stderr is always captured.
pub struct Pipe {
    read_pipe: bool,
    handle: HANDLE,
    handle_err: HANDLE,
    process: HANDLE,
}

fn close(handle: HANDLE) {
    // SAFETY: every handle passed here was returned by CreatePipe or
    // CreateProcessW and is closed once.
    unsafe {
        CloseHandle(handle);
    }
}

/// `create_pipe`: an anonymous pipe whose ends are inheritable.
fn create_pipe() -> Option<(HANDLE, HANDLE)> {
    let sa = SECURITY_ATTRIBUTES {
        nLength: size_of::<SECURITY_ATTRIBUTES>() as u32,
        lpSecurityDescriptor: ptr::null_mut(),
        bInheritHandle: 1,
    };
    let (mut input, mut output) = (ptr::null_mut(), ptr::null_mut());
    // SAFETY: both out pointers are valid locals and `sa` outlives the call.
    if unsafe { CreatePipe(&mut input, &mut output, &sa, 0) } == 0 {
        return None;
    }
    Some((input, output))
}

/// Clears `HANDLE_FLAG_INHERIT`, so the child does not get our end.
fn keep_private(handle: HANDLE) -> bool {
    // SAFETY: `handle` is an open pipe end.
    unsafe { SetHandleInformation(handle, HANDLE_FLAG_INHERIT, 0) != 0 }
}

/// `os_utf8_to_wcs_ptr` over the Windows `utf8_to_wchar`: a UTF-8 BOM is
/// dropped, invalid input becomes U+FFFD (`MultiByteToWideChar` with no
/// flags), and a failed conversion is an empty string. NUL-terminated.
fn utf8_to_wide(text: &[u8]) -> Vec<u16> {
    let text = text.strip_prefix(&[0xef, 0xbb, 0xbf][..]).unwrap_or(text);
    let mut wide = Vec::new();
    let len = i32::try_from(text.len()).unwrap_or(0);
    if len > 0 {
        // SAFETY: `text` covers `len` bytes; a null output asks for the size.
        let size =
            unsafe { MultiByteToWideChar(CP_UTF8, 0, text.as_ptr(), len, ptr::null_mut(), 0) };
        if size > 0 {
            wide.resize(size as usize, 0);
            // SAFETY: `wide` holds `size` units.
            let done = unsafe {
                MultiByteToWideChar(CP_UTF8, 0, text.as_ptr(), len, wide.as_mut_ptr(), size)
            };
            wide.truncate(done.max(0) as usize);
        }
    }
    wide.push(0);
    wide
}

/// `create_process`: runs `cmd_line` with the given stdio handles (null
/// where not redirected), inheriting every inheritable handle, without a
/// console window or the busy cursor. On failure `log` gets
/// `GetLastError()`.
fn create_process(
    cmd_line: &[u8],
    stdin: HANDLE,
    stdout: HANDLE,
    stderr: HANDLE,
    log: impl FnOnce(u32),
) -> Option<HANDLE> {
    let mut cmd_line = utf8_to_wide(cmd_line);
    // SAFETY: STARTUPINFOW and PROCESS_INFORMATION are plain C structs for
    // which all-zero is the documented initial state.
    let mut si: STARTUPINFOW = unsafe { core::mem::zeroed() };
    let mut pi: PROCESS_INFORMATION = unsafe { core::mem::zeroed() };
    si.cb = size_of::<STARTUPINFOW>() as u32;
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_FORCEOFFFEEDBACK;
    si.hStdInput = stdin;
    si.hStdOutput = stdout;
    si.hStdError = stderr;

    // SAFETY: `cmd_line` is a writable NUL-terminated buffer, as
    // CreateProcessW requires, and `si`/`pi` are valid for the call.
    let ok = unsafe {
        CreateProcessW(
            ptr::null(),
            cmd_line.as_mut_ptr(),
            ptr::null(),
            ptr::null(),
            1,
            CREATE_NO_WINDOW,
            ptr::null(),
            ptr::null(),
            &si,
            &mut pi,
        )
    };
    if ok == 0 {
        // Not logging the command line is intentional (C): it may hold
        // stream keys.
        // SAFETY: reads the calling thread's last error.
        log(unsafe { GetLastError() });
        return None;
    }
    close(pi.hThread);
    Some(pi.hProcess)
}

impl Pipe {
    /// Mirrors `os_process_pipe_create`, without logging spawn failures.
    pub fn create(cmd_line: Option<&[u8]>, type_: &[u8]) -> Option<Self> {
        Self::create_logged(cmd_line, type_, |_| {})
    }

    /// Mirrors `os_process_pipe_create`: `type_` must start with `r`
    /// (read the child's stdout) or `w` (write its stdin). `log` gets the
    /// error code when `CreateProcessW` fails, which C logs.
    pub fn create_logged(
        cmd_line: Option<&[u8]>,
        type_: &[u8],
        log: impl FnOnce(u32),
    ) -> Option<Self> {
        let cmd_line = cmd_line?;
        let read_pipe = match type_.first() {
            Some(b'r') => true,
            Some(b'w') => false,
            _ => return None,
        };
        let (input, output) = create_pipe()?;
        let Some((err_input, err_output)) = create_pipe() else {
            close(output);
            close(input);
            return None;
        };
        let close_all = || {
            close(output);
            close(input);
            close(err_output);
            close(err_input);
        };

        // Our ends: stdout's read end or stdin's write end, and stderr's
        // read end.
        let (ours, theirs) = if read_pipe {
            (input, output)
        } else {
            (output, input)
        };
        if !keep_private(ours) || !keep_private(err_input) {
            close_all();
            return None;
        }

        let (stdin, stdout) = if read_pipe {
            (ptr::null_mut(), output)
        } else {
            (input, ptr::null_mut())
        };
        let Some(process) = create_process(cmd_line, stdin, stdout, err_output, log) else {
            close_all();
            return None;
        };

        close(theirs);
        close(err_output);
        Some(Self {
            read_pipe,
            handle: ours,
            handle_err: err_input,
            process,
        })
    }

    /// Mirrors `os_process_pipe_create2`, without logging spawn failures.
    pub fn create2(args: Option<&Args>, type_: &[u8]) -> Option<Self> {
        Self::create2_logged(args, type_, |_| {})
    }

    /// Mirrors `os_process_pipe_create2`: Windows has no argv API, so the
    /// arguments are quoted into one command line
    /// ([`windows_command_line`]). A null `args` is `None`; C would
    /// dereference it.
    pub fn create2_logged(
        args: Option<&Args>,
        type_: &[u8],
        log: impl FnOnce(u32),
    ) -> Option<Self> {
        let cmd_line = windows_command_line(args?);
        Self::create_logged(Some(&cmd_line), type_, log)
    }

    /// Mirrors `os_process_pipe_destroy`: closes our ends, waits for the
    /// child and returns its exit code as `(int)DWORD`, or 0 if it cannot
    /// be read.
    pub fn destroy(self) -> c_int {
        close(self.handle);
        close(self.handle_err);
        let mut code = 0u32;
        // SAFETY: `process` is the child's open process handle and `code`
        // a valid local.
        let ret = unsafe {
            WaitForSingleObject(self.process, INFINITE);
            if GetExitCodeProcess(self.process, &mut code) != 0 {
                code as c_int
            } else {
                0
            }
        };
        close(self.process);
        ret
    }

    /// One `ReadFile` on `handle`; 0 on error or end of file. The length is
    /// cast to `DWORD` as in C.
    fn read_from(handle: HANDLE, buf: &mut [u8]) -> usize {
        let mut read = 0u32;
        // SAFETY: `buf` covers the length passed (truncated like C's cast)
        // and `read` is a valid local.
        let ok = unsafe {
            ReadFile(
                handle,
                buf.as_mut_ptr(),
                buf.len() as u32,
                &mut read,
                ptr::null_mut(),
            )
        };
        if ok != 0 { read as usize } else { 0 }
    }

    /// Mirrors `os_process_pipe_read`: one read of the child's stdout; 0 for
    /// a write-mode pipe.
    pub fn read(&mut self, buf: &mut [u8]) -> usize {
        if !self.read_pipe {
            return 0;
        }
        Self::read_from(self.handle, buf)
    }

    /// Mirrors `os_process_pipe_read_err`: one read of the child's stderr,
    /// in either mode.
    pub fn read_err(&mut self, buf: &mut [u8]) -> usize {
        if self.handle_err.is_null() {
            return 0;
        }
        Self::read_from(self.handle_err, buf)
    }

    /// Mirrors `os_process_pipe_write`: one `WriteFile` to the child's
    /// stdin (a short write is returned, not retried); 0 for a read-mode
    /// pipe or on error.
    pub fn write(&mut self, buf: &[u8]) -> usize {
        if self.read_pipe {
            return 0;
        }
        let mut written = 0u32;
        // SAFETY: `buf` covers the length passed (truncated like C's cast)
        // and `written` is a valid local.
        let ok = unsafe {
            WriteFile(
                self.handle,
                buf.as_ptr(),
                buf.len() as u32,
                &mut written,
                ptr::null_mut(),
            )
        };
        if ok != 0 { written as usize } else { 0 }
    }
}
