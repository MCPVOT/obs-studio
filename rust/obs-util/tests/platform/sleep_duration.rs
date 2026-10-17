//! Tier 1: the `os_sleep_ms` duration rule of `platform-windows.c`
//! (obs-rust/obs-studio#1274). Host-neutral, so it runs on every OS.
//!
//! A positive request must never become `Sleep(0)`, which only yields and
//! turns 1 ms retry loops into busy spins. The C file carries the same fix,
//! and the oracle compiles this repo's C, so there is no difference from C.

use obs_util::platform::win_sleep_duration;

const WIN7: u32 = 0x0601;
const WIN8: u32 = 0x0602;
const WIN10: u32 = 0x0A00;

#[test]
fn one_ms_never_becomes_zero() {
    assert_eq!(win_sleep_duration(1, WIN8), 1);
    assert_eq!(win_sleep_duration(1, WIN10), 1);
}

#[test]
fn zero_stays_zero() {
    assert_eq!(win_sleep_duration(0, WIN10), 0);
}

#[test]
fn larger_values_keep_the_win8_shortening() {
    assert_eq!(win_sleep_duration(2, WIN8), 1);
    assert_eq!(win_sleep_duration(10, WIN10), 9);
    assert_eq!(win_sleep_duration(u32::MAX, WIN10), u32::MAX - 1);
}

#[test]
fn pre_win8_is_unchanged() {
    for ms in [0, 1, 2, 10] {
        assert_eq!(win_sleep_duration(ms, WIN7), ms);
    }
}
