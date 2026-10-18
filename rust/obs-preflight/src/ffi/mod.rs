//! Narrow C ABI for the headless preflight engine.
//!
//! All decision logic stays in the safe crate API. This module only owns the
//! opaque engine handle, copies fixed-layout samples, and copies completed
//! results into caller-owned storage.

use core::ffi::c_char;
use core::ptr;

use crate::{
    DEFAULT_WINDOW_DURATION_MS, StatSample, Verdict, WindowResult, judge_window, roll_up_windows,
};

pub const STATUS_OK: i32 = 0;
pub const STATUS_INVALID_ARGUMENT: i32 = 1;
pub const STATUS_ANALYSIS_ERROR: i32 = 2;
pub const STATUS_OUT_OF_RANGE: i32 = 3;
use obs_preflight::ffi_types::REASON_CAPACITY;

pub use obs_preflight::ffi_types::{ObsPreflightSample, ObsPreflightWindow};

/// Opaque, independently owned history for one preflight run.
pub struct PreflightEngine {
    window_duration_ms: u64,
    samples: Vec<StatSample>,
}

fn results(engine: &PreflightEngine) -> Result<Vec<WindowResult>, ()> {
    roll_up_windows(&engine.samples, engine.window_duration_ms)
        .map(|windows| windows.into_iter().map(judge_window).collect())
        .map_err(|_| ())
}

fn copy_result(source: &WindowResult) -> ObsPreflightWindow {
    let rollup = &source.rollup;
    let mut result = ObsPreflightWindow {
        start_ms: rollup.start_ms,
        end_ms: rollup.end_ms,
        duration_ms: rollup.duration_ms,
        output_bitrate_kbps: rollup.output_bitrate_kbps,
        output_bytes: rollup.output_bytes,
        output_frames: rollup.output_frames,
        output_frames_dropped: rollup.output_frames_dropped,
        output_drop_percent: rollup.output_drop_percent,
        encoder_frames: rollup.encoder_frames,
        encoder_frames_skipped: rollup.encoder_frames_skipped,
        encoder_skip_percent: rollup.encoder_skip_percent,
        lagged_frames: rollup.lagged_frames,
        render_lag_percent: rollup.render_lag_percent,
        average_frame_time_ns: rollup.average_frame_time_ns,
        frame_interval_ns: rollup.frame_interval_ns,
        render_headroom_ns: rollup.render_headroom_ns,
        congestion_percent: rollup.congestion_percent,
        connect_time_ms: rollup.connect_time_ms,
        cpu_percent: rollup.cpu_percent,
        verdict: match source.verdict {
            Verdict::Pass => 0,
            Verdict::Marginal => 1,
            Verdict::Fail => 2,
        },
        reason: [0; REASON_CAPACITY],
    };
    // All verdict reasons are plain English. Keep one byte for the C NUL.
    for (destination, source) in result
        .reason
        .iter_mut()
        .zip(source.reason.as_bytes().iter().take(REASON_CAPACITY - 1))
    {
        *destination = *source as c_char;
    }
    result
}

/// Creates an engine using the standard five-second window.
#[unsafe(no_mangle)]
pub extern "C" fn obs_preflight_engine_create() -> *mut PreflightEngine {
    Box::into_raw(Box::new(PreflightEngine {
        window_duration_ms: DEFAULT_WINDOW_DURATION_MS,
        samples: Vec::new(),
    }))
}

/// Creates an engine with a caller-selected window duration.
///
/// Returns null when `window_duration_ms` is zero.
#[unsafe(no_mangle)]
pub extern "C" fn obs_preflight_engine_create_with_window(
    window_duration_ms: u64,
) -> *mut PreflightEngine {
    if window_duration_ms == 0 {
        return ptr::null_mut();
    }
    Box::into_raw(Box::new(PreflightEngine {
        window_duration_ms,
        samples: Vec::new(),
    }))
}

/// Destroys an engine previously returned by either create function. Null is accepted.
///
/// # Safety
///
/// A non-null `engine` must be a live handle returned by this module and must
/// not be used again after this call.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_preflight_engine_destroy(engine: *mut PreflightEngine) {
    if !engine.is_null() {
        // SAFETY: guaranteed by the function contract.
        drop(unsafe { Box::from_raw(engine) });
    }
}

/// Copies a sample into the engine. Counters are cumulative within a run. Any
/// decrease starts a new result window and omits the interval crossing reset.
///
/// # Safety
///
/// `engine` must be a live handle returned by this module and `sample` must
/// point to a readable `ObsPreflightSample`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_preflight_engine_push_sample(
    engine: *mut PreflightEngine,
    sample: *const ObsPreflightSample,
) -> i32 {
    if engine.is_null() || sample.is_null() {
        return STATUS_INVALID_ARGUMENT;
    }
    // SAFETY: the caller provides a live engine and readable sample pointer.
    let sample = unsafe { *sample };
    // SAFETY: the caller provides exclusive mutable access to the engine.
    unsafe { &mut *engine }.samples.push(sample.into());
    STATUS_OK
}

/// Writes the number of completed windows to `count_out`.
///
/// Returns `STATUS_ANALYSIS_ERROR` until at least two valid samples exist.
///
/// # Safety
///
/// `engine` must be a live handle and `count_out` must point to writable
/// storage for one `size_t` value.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_preflight_engine_get_window_count(
    engine: *const PreflightEngine,
    count_out: *mut usize,
) -> i32 {
    if engine.is_null() || count_out.is_null() {
        return STATUS_INVALID_ARGUMENT;
    }
    // SAFETY: the caller provides a live engine.
    let windows = match results(unsafe { &*engine }) {
        Ok(windows) => windows,
        Err(()) => return STATUS_ANALYSIS_ERROR,
    };
    // SAFETY: the caller provides writable output storage.
    unsafe { count_out.write(windows.len()) };
    STATUS_OK
}

/// Writes one rollup and verdict into caller-owned storage.
///
/// # Safety
///
/// `engine` must be a live handle and `window_out` must point to writable
/// storage for one `ObsPreflightWindow`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_preflight_engine_get_window(
    engine: *const PreflightEngine,
    index: usize,
    window_out: *mut ObsPreflightWindow,
) -> i32 {
    if engine.is_null() || window_out.is_null() {
        return STATUS_INVALID_ARGUMENT;
    }
    // SAFETY: the caller provides a live engine.
    let windows = match results(unsafe { &*engine }) {
        Ok(windows) => windows,
        Err(()) => return STATUS_ANALYSIS_ERROR,
    };
    let Some(window) = windows.get(index) else {
        return STATUS_OUT_OF_RANGE;
    };
    // SAFETY: the caller provides writable output storage.
    unsafe { window_out.write(copy_result(window)) };
    STATUS_OK
}
