//! Original `platform-nix-dbus.c.h` linked under `oracle_dbus_*`, with a
//! deterministic GIO boundary for Tier 3 parity.

use core::ffi::{c_char, c_void};

unsafe extern "C" {
    pub fn oracle_dbus_stub_config(mask: u32, fail_connection: bool, fail_call: bool, cookie: u32);
    pub fn oracle_dbus_stub_take_trace(out: *mut c_char, capacity: usize) -> usize;
    pub fn oracle_dbus_sleep_info_create() -> *mut c_void;
    pub fn oracle_dbus_inhibit_sleep(info: *mut c_void, reason: *const c_char, active: bool);
    pub fn oracle_dbus_sleep_info_destroy(info: *mut c_void);
}
