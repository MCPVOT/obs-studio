//! `oracle_*` build of `libobs/util/source-profiler.c.h`
//! (oracle/source_profiler.c), with deterministic stand-ins for the clock,
//! GPU timers, the graphics context and the task queue, and accessors for
//! its state. One process-wide profiler: callers serialize.

use core::ffi::c_void;

/// `profiler_result_t`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleProfilerResult {
    pub tick_avg: u64,
    pub tick_max: u64,
    pub render_avg: u64,
    pub render_max: u64,
    pub render_gpu_avg: u64,
    pub render_gpu_max: u64,
    pub render_sum: u64,
    pub render_gpu_sum: u64,
    pub async_input: f64,
    pub async_rendered: f64,
    pub async_input_best: u64,
    pub async_input_worst: u64,
    pub async_rendered_best: u64,
    pub async_rendered_worst: u64,
}

/// The head of `struct obs_video_info`.
#[repr(C)]
pub struct OracleVideoInfoHead {
    pub graphics_module: *const core::ffi::c_char,
    pub fps_num: u32,
    pub fps_den: u32,
}

unsafe extern "C" {
    pub fn oracle_source_profiler_enable(enable: bool);
    pub fn oracle_source_profiler_gpu_enable(enable: bool);
    pub fn oracle_source_profiler_get_result(source: *mut c_void) -> *mut OracleProfilerResult;
    pub fn oracle_source_profiler_fill_result(
        source: *mut c_void,
        result: *mut OracleProfilerResult,
    ) -> bool;
    pub fn oracle_source_profiler_reset_video(ovi: *const OracleVideoInfoHead);
    pub fn oracle_source_profiler_render_begin();
    pub fn oracle_source_profiler_render_end();
    pub fn oracle_source_profiler_frame_begin();
    pub fn oracle_source_profiler_frame_collect();
    pub fn oracle_source_profiler_async_frame_received(source: *mut c_void);
    pub fn oracle_source_profiler_source_tick_start() -> u64;
    pub fn oracle_source_profiler_source_tick_end(source: *mut c_void, start: u64);
    pub fn oracle_source_profiler_source_render_begin(timer: *mut *mut c_void) -> u64;
    pub fn oracle_source_profiler_source_render_end(
        source: *mut c_void,
        start: u64,
        timer: *mut c_void,
    );
    pub fn oracle_source_profiler_remove_source(source: *mut c_void);

    pub fn oracle_sp_reset();
    pub fn oracle_sp_gettime_ns() -> u64;
    pub fn oracle_sp_set_clock_step(step: u64);
    pub fn oracle_sp_push_time(t: u64);
    pub fn oracle_sp_pending_times() -> usize;
    pub fn oracle_sp_set_next_ticks(ok: bool, ticks: u64);
    pub fn oracle_sp_set_range(disjoint: bool, frequency: u64);
    pub fn oracle_sp_set_null_handles(null_handles: bool);
    pub fn oracle_sp_context_depth() -> core::ffi::c_long;
    pub fn oracle_sp_warnings() -> usize;
    pub fn oracle_sp_take_destroyed_timers(out: *mut usize, cap: usize) -> usize;
    pub fn oracle_sp_take_destroyed_ranges(out: *mut usize, cap: usize) -> usize;
    pub fn oracle_sp_source_new(output_flags: u32) -> *mut c_void;
    pub fn oracle_sp_source_set_last_async_ts(source: *mut c_void, ts: u64);
    pub fn oracle_sp_source_free(source: *mut c_void);

    pub fn oracle_sp_state(
        enabled: *mut bool,
        gpu: *mut bool,
        idx: *mut u8,
        ranges: *mut usize,
        samples: *mut u64,
    );
    pub fn oracle_sp_frame_buffer_size() -> usize;
    pub fn oracle_sp_sample(
        source: *const c_void,
        frame: usize,
        frame_idx: *mut u8,
        tick: *mut u64,
        cpu: *mut u64,
        cpu_cap: usize,
        cpu_num: *mut usize,
        timers: *mut usize,
        timers_cap: usize,
        timers_num: *mut usize,
    ) -> bool;
    pub fn oracle_sp_samples_count() -> usize;
    pub fn oracle_sp_entries_count() -> usize;
    pub fn oracle_sp_entry_ring(
        source: *const c_void,
        which: core::ffi::c_int,
        out: *mut u64,
        cap: usize,
        num: *mut usize,
        idx: *mut usize,
        capacity: *mut usize,
    ) -> bool;
    pub fn oracle_sp_ucirclebuf_run(
        capacity: usize,
        values: *const u64,
        count: usize,
        out: *mut u64,
        num: *mut usize,
        idx: *mut usize,
    );
    #[allow(clippy::too_many_arguments)]
    pub fn oracle_sp_calculate(
        tick: *mut u64,
        tick_num: usize,
        cpu: *mut u64,
        cpu_sum: *mut u64,
        cpu_num: usize,
        gpu: *mut u64,
        gpu_sum: *mut u64,
        gpu_num: usize,
        gpu_on: bool,
        frames: *mut u64,
        frames_num: usize,
        result: *mut OracleProfilerResult,
    );
    pub fn oracle_sp_result_size() -> usize;
    pub fn oracle_sp_result_align() -> usize;
    pub fn oracle_sp_result_offset(field: core::ffi::c_int) -> usize;
    pub fn oracle_sp_ovi_fps_num_offset() -> usize;
    pub fn oracle_sp_ovi_fps_den_offset() -> usize;
}
