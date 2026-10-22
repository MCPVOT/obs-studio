//! Differential driver for `libobs/util/platform-nix-portal.c.h`.
//! The C file is compiled with its GIO calls replaced. Linux and FreeBSD.

use core::ffi::{c_char, c_void};
use std::ffi::CStr;

unsafe extern "C" {
    pub fn oracle_portal_stub_config(bus_missing: bool, portal_missing: bool);
    pub fn oracle_portal_inhibit_info_create() -> *mut c_void;
    pub fn oracle_portal_inhibit(info: *mut c_void, reason: *const c_char, active: bool);
    pub fn oracle_portal_inhibit_info_destroy(info: *mut c_void);
    pub fn oracle_portal_complete(error: bool);
    pub fn oracle_portal_respond(code: u32);
    pub fn oracle_portal_stub_take_trace(out: *mut c_char, capacity: usize) -> usize;
}

pub struct Oracle {
    info: *mut c_void,
}

impl Oracle {
    pub fn start(bus_missing: bool, portal_missing: bool) -> Self {
        // SAFETY: the stand-in state is process-global. The parity test
        // holds one lock around each case.
        unsafe { oracle_portal_stub_config(bus_missing, portal_missing) };
        Self {
            info: core::ptr::null_mut(),
        }
    }

    pub fn create(&mut self) {
        // SAFETY: no arguments. NULL means the portal was not found.
        self.info = unsafe { oracle_portal_inhibit_info_create() };
    }

    pub fn inhibit(&mut self, reason: &CStr, active: bool) {
        if self.info.is_null() {
            return;
        }
        // SAFETY: `info` came from create and has not been destroyed.
        unsafe { oracle_portal_inhibit(self.info, reason.as_ptr(), active) };
    }

    pub fn complete(&mut self, error: bool) {
        // SAFETY: completes the oldest queued async call, if there is one.
        unsafe { oracle_portal_complete(error) };
    }

    pub fn respond(&mut self, code: u32) {
        // SAFETY: delivers one Response signal to the stored callback.
        unsafe { oracle_portal_respond(code) };
    }

    pub fn destroy(&mut self) {
        // SAFETY: NULL, or the pointer from create, consumed once.
        unsafe { oracle_portal_inhibit_info_destroy(self.info) };
        self.info = core::ptr::null_mut();
    }

    pub fn trace(&self) -> String {
        let mut buf = vec![0u8; 65536];
        // SAFETY: the buffer is writable and one byte is reserved for NUL.
        let length = unsafe { oracle_portal_stub_take_trace(buf.as_mut_ptr().cast(), buf.len()) };
        assert!(length < buf.len(), "portal trace was truncated");
        buf.truncate(length);
        String::from_utf8(buf).expect("portal trace is utf-8")
    }
}
