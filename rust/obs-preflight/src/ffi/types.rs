//! Shared data layouts consumed by the C adapter and ABI tests.

use core::ffi::c_char;

pub const REASON_CAPACITY: usize = 256;

/// Mirrors `struct obs_preflight_sample` in the hand-written C header.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ObsPreflightSample {
    pub timestamp_ms: u64,
    pub average_frame_time_ns: u64,
    pub frame_interval_ns: u64,
    pub lagged_frames: u32,
    pub output_total_bytes: u64,
    pub output_total_frames: u64,
    pub output_frames_dropped: u64,
    pub output_congestion: f64,
    pub connect_time_ms: i32,
    pub encoder_total_frames: u64,
    pub encoder_skipped_frames: u64,
    pub cpu_percent: f64,
}

/// Mirrors `struct obs_preflight_window` in the hand-written C header.
#[repr(C)]
#[derive(Clone, Copy, Debug)]
pub struct ObsPreflightWindow {
    pub start_ms: u64,
    pub end_ms: u64,
    pub duration_ms: u64,
    pub output_bitrate_kbps: f64,
    pub output_bytes: u64,
    pub output_frames: u64,
    pub output_frames_dropped: u64,
    pub output_drop_percent: f64,
    pub encoder_frames: u64,
    pub encoder_frames_skipped: u64,
    pub encoder_skip_percent: f64,
    pub lagged_frames: u64,
    pub render_lag_percent: f64,
    pub average_frame_time_ns: f64,
    pub frame_interval_ns: f64,
    pub render_headroom_ns: i64,
    pub congestion_percent: f64,
    pub connect_time_ms: i32,
    pub cpu_percent: f64,
    pub verdict: i32,
    pub reason: [c_char; REASON_CAPACITY],
}

impl From<ObsPreflightSample> for crate::StatSample {
    fn from(sample: ObsPreflightSample) -> Self {
        Self {
            timestamp_ms: sample.timestamp_ms,
            average_frame_time_ns: sample.average_frame_time_ns,
            frame_interval_ns: sample.frame_interval_ns,
            lagged_frames: sample.lagged_frames,
            output_total_bytes: sample.output_total_bytes,
            output_total_frames: sample.output_total_frames,
            output_frames_dropped: sample.output_frames_dropped,
            output_congestion: sample.output_congestion,
            connect_time_ms: sample.connect_time_ms,
            encoder_total_frames: sample.encoder_total_frames,
            encoder_skipped_frames: sample.encoder_skipped_frames,
            cpu_percent: sample.cpu_percent,
        }
    }
}
