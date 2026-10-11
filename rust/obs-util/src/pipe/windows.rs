//! Windows half of the pipe port: `libobs/util/pipe-windows.c`.
//!
//! RED: the API the shim and tests use, with no implementation yet.

use core::ffi::c_int;

use super::Args;

/// A spawned child with its stdio pipes. Mirrors `struct os_process_pipe` in
/// `pipe-windows.c`.
pub struct Pipe {
    _private: (),
}

impl Pipe {
    /// Mirrors `os_process_pipe_create`, without logging spawn failures.
    pub fn create(cmd_line: Option<&[u8]>, type_: &[u8]) -> Option<Self> {
        Self::create_logged(cmd_line, type_, |_| {})
    }

    /// Mirrors `os_process_pipe_create`.
    pub fn create_logged(
        _cmd_line: Option<&[u8]>,
        _type_: &[u8],
        _log: impl FnOnce(u32),
    ) -> Option<Self> {
        todo!("port pipe-windows.c os_process_pipe_create")
    }

    /// Mirrors `os_process_pipe_create2`, without logging spawn failures.
    pub fn create2(args: Option<&Args>, type_: &[u8]) -> Option<Self> {
        Self::create2_logged(args, type_, |_| {})
    }

    /// Mirrors `os_process_pipe_create2`.
    pub fn create2_logged(
        _args: Option<&Args>,
        _type_: &[u8],
        _log: impl FnOnce(u32),
    ) -> Option<Self> {
        todo!("port pipe-windows.c os_process_pipe_create2")
    }

    /// Mirrors `os_process_pipe_destroy`.
    pub fn destroy(self) -> c_int {
        todo!("port pipe-windows.c os_process_pipe_destroy")
    }

    /// Mirrors `os_process_pipe_read`.
    pub fn read(&mut self, _buf: &mut [u8]) -> usize {
        todo!("port pipe-windows.c os_process_pipe_read")
    }

    /// Mirrors `os_process_pipe_read_err`.
    pub fn read_err(&mut self, _buf: &mut [u8]) -> usize {
        todo!("port pipe-windows.c os_process_pipe_read_err")
    }

    /// Mirrors `os_process_pipe_write`.
    pub fn write(&mut self, _buf: &[u8]) -> usize {
        todo!("port pipe-windows.c os_process_pipe_write")
    }
}
