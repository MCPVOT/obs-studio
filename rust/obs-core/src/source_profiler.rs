//! Safe core of `libobs/util/source-profiler.c.h`: per-source tick and
//! render timing, and the fps of async sources.
//!
//! The graphics thread drives it once per frame, as `obs-video.c` does:
//! [`SourceProfiler::frame_begin`], the tick and render hooks of every
//! source, [`SourceProfiler::frame_collect`]. A frame's samples are kept in
//! a [`FRAME_BUFFER_SIZE`] ring per source and collected one frame later,
//! which gives the GPU timers time to finish. Collection pushes into the
//! source's [`Entry`]: fixed-size rings ([`CircleBuf`]) of the last five
//! seconds of frames. [`SourceProfiler::fill_result`] turns an entry into a
//! [`ProfilerResult`] for any thread.
//!
//! Everything libobs supplies (the clock, GPU timers, the graphics context,
//! a source's flags and last rendered async timestamp, the log line and
//! the profiler scope of a collection) goes through [`Host`]. Sources and
//! GPU timers are opaque [`Handle`]s; 0 is NULL.
//!
//! Kept as the C has it, characterized and not endorsed:
//!
//! - a source that is not ticked in a frame keeps its last tick in the
//!   sample ring, and collection pushes that stale tick again;
//! - GPU timers left over after GPU timing was disabled are destroyed
//!   without the graphics context (the device ignores that);
//! - sums and averages wrap like the C's `uint64_t`;
//! - `gpu_enable(true)` while the profiler is not enabled is dropped.
//!
//! Intentional differences (also in rust/README.md and #1886):
//!
//! - a [`CircleBuf`] of capacity 0 drops pushes, and a GPU frequency of 0
//!   converts to 0 ns; the C divides by zero in both;
//! - removing a source takes the entry table's write lock; the C mutates it
//!   under the read lock;
//! - the graphics context is never waited for while the sample table is
//!   locked: timers of destroyed samples are destroyed after the lock is
//!   released, in one enter/leave instead of one per sample.

use std::collections::BTreeMap;
use std::sync::atomic::{AtomicBool, AtomicU64, Ordering};
use std::sync::{Mutex, MutexGuard, PoisonError, RwLock, RwLockReadGuard, RwLockWriteGuard};

use obs_util::util_uint64::mul_div64;

/// Frames a sample waits before it is collected: `NUM_TEXTURES` in
/// `obs-internal.h`, the depth of the render buffer.
pub const FRAME_BUFFER_SIZE: usize = 2;

/// Sources are rendered more than once per frame at times; room for two
/// passes is kept per frame sample.
const RENDER_TIMES_RESERVATION: usize = 2;

/// An opaque libobs pointer (a source, a `gs_timer_t`, a
/// `gs_timer_range_t`) as an address. 0 is NULL.
pub type Handle = usize;

/// What `gs_timer_range_get_data` reports. The C starts from `false` and 0
/// and keeps them when the device does not write them.
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct RangeData {
    pub disjoint: bool,
    pub frequency: u64,
}

/// What the profiler needs from libobs.
pub trait Host {
    /// `os_gettime_ns`.
    fn now_ns(&self) -> u64;
    /// `gs_enter_context(obs->video.graphics)`.
    fn enter_graphics(&self);
    /// `gs_leave_context`.
    fn leave_graphics(&self);
    /// `gs_timer_create`; 0 for NULL.
    fn timer_create(&self) -> Handle;
    fn timer_begin(&self, timer: Handle);
    fn timer_end(&self, timer: Handle);
    /// `gs_timer_get_data`: the ticks, or `None` when it returns false.
    fn timer_get_data(&self, timer: Handle) -> Option<u64>;
    fn timer_destroy(&self, timer: Handle);
    /// `gs_timer_range_create`; 0 for NULL.
    fn range_create(&self) -> Handle;
    fn range_begin(&self, range: Handle);
    fn range_end(&self, range: Handle);
    fn range_get_data(&self, range: Handle) -> RangeData;
    fn range_destroy(&self, range: Handle);
    /// The source's output flags include `OBS_SOURCE_ASYNC_VIDEO`.
    fn is_async_video(&self, source: Handle) -> bool;
    /// `obs_source_get_last_async_ts`.
    fn last_async_ts(&self, source: Handle) -> u64;
    /// The "GPU Timers were disjoint, discarding samples." warning.
    fn warn_disjoint(&self);
    /// `profile_start` / `profile_end` of `source_profiler_frame_collect`.
    fn collect_scope_begin(&self);
    fn collect_scope_end(&self);
}

/// `struct ucirclebuf`: the most recent `capacity` values; older ones are
/// overwritten. [`CircleBuf::values`] is the storage order, as the C walks
/// `array[0..num]`.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct CircleBuf {
    idx: usize,
    capacity: usize,
    values: Vec<u64>,
}

impl CircleBuf {
    /// `ucirclebuf_init`.
    pub fn new(capacity: usize) -> Self {
        Self {
            idx: 0,
            capacity,
            values: Vec::with_capacity(capacity),
        }
    }

    /// `ucirclebuf_push`. Capacity 0 drops the value (the C divides by
    /// zero).
    pub fn push(&mut self, value: u64) {
        if self.capacity == 0 {
            return;
        }
        if self.values.len() == self.capacity {
            self.idx %= self.capacity;
            if let Some(slot) = self.values.get_mut(self.idx) {
                *slot = value;
            }
        } else {
            self.values.push(value);
        }
        self.idx = self.idx.wrapping_add(1);
    }

    /// `array[0..num]`.
    pub fn values(&self) -> &[u64] {
        &self.values
    }

    /// The next write position before wrapping (`idx`).
    pub fn idx(&self) -> usize {
        self.idx
    }

    pub fn capacity(&self) -> usize {
        self.capacity
    }

    /// `num`.
    pub fn len(&self) -> usize {
        self.values.len()
    }

    pub fn is_empty(&self) -> bool {
        self.values.is_empty()
    }
}

/// `profiler_result_t`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct ProfilerResult {
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

/// Average and maximum of `values` (`calculate_tick`): `None` for no
/// values. The sum wraps like the C's.
pub fn average_max(values: &[u64]) -> Option<(u64, u64)> {
    let count = u64::try_from(values.len()).ok().filter(|&n| n != 0)?;
    let mut sum = 0u64;
    let mut max = 0u64;
    for &v in values {
        max = max.max(v);
        sum = sum.wrapping_add(v);
    }
    Some((sum.checked_div(count)?, max))
}

/// `calculate_tick`: writes `tick_avg` and `tick_max`.
pub fn calculate_tick(ticks: &[u64], result: &mut ProfilerResult) {
    if let Some((avg, max)) = average_max(ticks) {
        result.tick_avg = avg;
        result.tick_max = max;
    }
}

/// The average, maximum and average sum of a first-pass ring and its
/// sum-of-passes ring, walked to the length of the first (the C indexes
/// the sum ring by the first ring's `num`; both are pushed together).
fn render_stats(first: &[u64], sums: &[u64]) -> Option<(u64, u64, u64)> {
    let count = u64::try_from(first.len()).ok().filter(|&n| n != 0)?;
    let mut sum = 0u64;
    let mut sum_sum = 0u64;
    let mut max = 0u64;
    for (i, &v) in first.iter().enumerate() {
        max = max.max(v);
        sum = sum.wrapping_add(v);
        sum_sum = sum_sum.wrapping_add(sums.get(i).copied().unwrap_or(0));
    }
    Some((sum.checked_div(count)?, max, sum_sum.checked_div(count)?))
}

/// `calculate_render`: the CPU fields, and the GPU fields when `gpu` is
/// given (GPU timing enabled).
pub fn calculate_render(
    cpu: &[u64],
    cpu_sum: &[u64],
    gpu: Option<(&[u64], &[u64])>,
    result: &mut ProfilerResult,
) {
    if let Some((avg, max, sum)) = render_stats(cpu, cpu_sum) {
        result.render_avg = avg;
        result.render_max = max;
        result.render_sum = sum;
    }
    if let Some((avg, max, sum)) = gpu.and_then(|(first, sums)| render_stats(first, sums)) {
        result.render_gpu_avg = avg;
        result.render_gpu_max = max;
        result.render_gpu_sum = sum;
    }
}

/// What `calculate_fps` reports.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct FpsStats {
    pub fps: f64,
    pub best: u64,
    pub worst: u64,
}

/// `calculate_fps` over timestamps in storage order: each timestamp's
/// delta to the one stored before it (the first to the last), skipping
/// zero or non-increasing pairs and stopping at the first zero timestamp.
/// `None` (the C writes nothing) when no delta counted or they sum to 0.
pub fn calculate_fps(frames: &[u64]) -> Option<FpsStats> {
    let last = frames.len().checked_sub(1)?;
    let mut deltas = 0u64;
    let mut delta_sum = 0u64;
    let mut best = 0u64;
    let mut worst = 0u64;
    for (idx, &ts) in frames.iter().enumerate() {
        if ts == 0 {
            break;
        }
        let prev_idx = idx.checked_sub(1).unwrap_or(last);
        let prev_ts = frames.get(prev_idx).copied().unwrap_or(0);
        if prev_ts == 0 || prev_ts >= ts {
            continue;
        }
        let delta = ts.wrapping_sub(prev_ts);
        if delta < best || best == 0 {
            best = delta;
        }
        worst = worst.max(delta);
        delta_sum = delta_sum.wrapping_add(delta);
        deltas = deltas.wrapping_add(1);
    }
    if deltas == 0 || delta_sum == 0 {
        return None;
    }
    #[allow(clippy::cast_precision_loss)]
    let fps = 1.0e9 / (delta_sum as f64 / deltas as f64);
    Some(FpsStats { fps, best, worst })
}

/// `struct frame_sample`: one frame of one source.
#[derive(Debug, Clone, Default, PartialEq, Eq)]
pub struct FrameSample {
    /// Tick time; 0 is "no data yet".
    pub tick: u64,
    /// CPU time of each render pass.
    pub render_cpu: Vec<u64>,
    /// GPU timer of each render pass.
    pub render_timers: Vec<Handle>,
}

impl FrameSample {
    fn new() -> Self {
        Self {
            tick: 0,
            render_cpu: Vec::with_capacity(RENDER_TIMES_RESERVATION),
            render_timers: Vec::with_capacity(RENDER_TIMES_RESERVATION),
        }
    }
}

/// `struct source_samples`: a source's last [`FRAME_BUFFER_SIZE`] frames.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct SourceSamples {
    /// The frame being written.
    pub frame_idx: usize,
    pub frames: [FrameSample; FRAME_BUFFER_SIZE],
}

impl SourceSamples {
    fn new() -> Self {
        Self {
            frame_idx: 0,
            frames: [FrameSample::new(), FrameSample::new()],
        }
    }

    fn current(&mut self) -> Option<&mut FrameSample> {
        self.frames.get_mut(self.frame_idx)
    }

    /// Every GPU timer still held, which destroying the samples destroys.
    fn take_timers(&mut self, out: &mut Vec<Handle>) {
        for frame in &mut self.frames {
            out.append(&mut frame.render_timers);
        }
    }
}

/// `struct profiler_entry`: a source's collected frames.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Entry {
    /// Tick times.
    pub tick: CircleBuf,
    /// Time of the first render pass of a frame.
    pub render_cpu: CircleBuf,
    pub render_gpu: CircleBuf,
    /// Sum of all render passes of a frame.
    pub render_cpu_sum: CircleBuf,
    pub render_gpu_sum: CircleBuf,
    /// Timestamps of async frame submissions.
    pub async_frame_ts: CircleBuf,
    /// Timestamps of the async frames rendered, one per frame.
    pub async_rendered_ts: CircleBuf,
}

impl Entry {
    fn new(capacity: usize) -> Self {
        Self {
            tick: CircleBuf::new(capacity),
            render_cpu: CircleBuf::new(capacity),
            render_gpu: CircleBuf::new(capacity),
            render_cpu_sum: CircleBuf::new(capacity),
            render_gpu_sum: CircleBuf::new(capacity),
            async_frame_ts: CircleBuf::new(capacity),
            async_rendered_ts: CircleBuf::new(capacity),
        }
    }
}

/// What [`SourceProfiler::fill_result`] found.
#[derive(Debug, Clone, Copy, PartialEq)]
pub enum Fill {
    /// The profiler is not enabled: false, the result is left alone.
    Disabled,
    /// No entry for the source: false, the result is zeroed.
    Missing,
    /// True, with the result.
    Found(ProfilerResult),
}

/// The graphics thread's state: `hm_samples` and the GPU timer ranges.
#[derive(Debug)]
struct Graphics {
    samples: BTreeMap<Handle, SourceSamples>,
    timer_idx: usize,
    timer_ranges: [Handle; FRAME_BUFFER_SIZE],
}

/// The profiler (the C file's statics).
#[derive(Debug)]
pub struct SourceProfiler {
    enabled: AtomicBool,
    gpu_enabled: AtomicBool,
    enable_next: AtomicBool,
    gpu_enable_next: AtomicBool,
    samples_per_entry: AtomicU64,
    graphics: Mutex<Graphics>,
    entries: RwLock<BTreeMap<Handle, Entry>>,
}

impl Default for SourceProfiler {
    fn default() -> Self {
        Self::new()
    }
}

impl SourceProfiler {
    pub const fn new() -> Self {
        Self {
            enabled: AtomicBool::new(false),
            gpu_enabled: AtomicBool::new(false),
            enable_next: AtomicBool::new(false),
            gpu_enable_next: AtomicBool::new(false),
            samples_per_entry: AtomicU64::new(0),
            graphics: Mutex::new(Graphics {
                samples: BTreeMap::new(),
                timer_idx: 0,
                timer_ranges: [0; FRAME_BUFFER_SIZE],
            }),
            entries: RwLock::new(BTreeMap::new()),
        }
    }

    fn graphics(&self) -> MutexGuard<'_, Graphics> {
        self.graphics.lock().unwrap_or_else(PoisonError::into_inner)
    }

    fn entries_read(&self) -> RwLockReadGuard<'_, BTreeMap<Handle, Entry>> {
        self.entries.read().unwrap_or_else(PoisonError::into_inner)
    }

    fn entries_write(&self) -> RwLockWriteGuard<'_, BTreeMap<Handle, Entry>> {
        self.entries.write().unwrap_or_else(PoisonError::into_inner)
    }

    /// The profiler collects (`sp_enabled`).
    pub fn is_enabled(&self) -> bool {
        self.enabled.load(Ordering::Relaxed)
    }

    /// GPU timing is on (`gpu_enabled`).
    pub fn is_gpu_enabled(&self) -> bool {
        self.gpu_enabled.load(Ordering::Relaxed)
    }

    /// Ring capacity of new entries: five seconds of frames.
    pub fn samples_per_entry(&self) -> u64 {
        self.samples_per_entry.load(Ordering::Relaxed)
    }

    /// `source_profiler_enable`: applied at the next frame.
    pub fn enable(&self, enable: bool) {
        self.enable_next.store(enable, Ordering::Relaxed);
    }

    /// `source_profiler_gpu_enable`: applied at the next frame, and only
    /// while an enable is pending or applied.
    pub fn gpu_enable(&self, enable: bool) {
        let next = enable && self.enable_next.load(Ordering::Relaxed);
        self.gpu_enable_next.store(next, Ordering::Relaxed);
    }

    /// `source_profiler_reset_video`: five seconds of frames at
    /// `ceil(fps_num / fps_den)` fps, and everything dropped. The video
    /// thread is not running.
    pub fn reset_video<H: Host>(&self, fps_num: u32, fps_den: u32, host: &H) {
        let fps = (f64::from(fps_num) / f64::from(fps_den)).ceil();
        #[allow(clippy::cast_possible_truncation, clippy::cast_sign_loss)]
        let samples = (fps * 5.0) as u64;
        self.samples_per_entry.store(samples, Ordering::Relaxed);
        self.shutdown(host);
    }

    /// `profiler_shutdown`: drop every sample and entry, destroy their
    /// timers and the timer ranges.
    fn shutdown<H: Host>(&self, host: &H) {
        let mut timers = Vec::new();
        {
            let mut graphics = self.graphics();
            for (_, mut samples) in std::mem::take(&mut graphics.samples) {
                samples.take_timers(&mut timers);
            }
        }
        destroy_timers(&timers, host);
        self.entries_write().clear();
        self.reset_gpu_timers(host);
    }

    /// `reset_gpu_timers`.
    fn reset_gpu_timers<H: Host>(&self, host: &H) {
        let ranges = std::mem::take(&mut self.graphics().timer_ranges);
        host.enter_graphics();
        for range in ranges {
            if range != 0 {
                host.range_destroy(range);
            }
        }
        host.leave_graphics();
    }

    /// `source_profiler_render_begin`: starts this frame's GPU timer range.
    pub fn render_begin<H: Host>(&self, host: &H) {
        if !self.is_gpu_enabled() {
            return;
        }
        host.enter_graphics();
        let range = {
            let mut graphics = self.graphics();
            let idx = graphics.timer_idx;
            match graphics.timer_ranges.get_mut(idx) {
                Some(slot) => {
                    if *slot == 0 {
                        *slot = host.range_create();
                    }
                    *slot
                }
                None => 0,
            }
        };
        host.range_begin(range);
        host.leave_graphics();
    }

    /// `source_profiler_render_end`.
    pub fn render_end<H: Host>(&self, host: &H) {
        if !self.is_gpu_enabled() {
            return;
        }
        let range = {
            let graphics = self.graphics();
            graphics
                .timer_ranges
                .get(graphics.timer_idx)
                .copied()
                .unwrap_or(0)
        };
        if range == 0 {
            return;
        }
        host.enter_graphics();
        host.range_end(range);
        host.leave_graphics();
    }

    /// `source_profiler_frame_begin`: applies a pending enable, and
    /// advances the timer range while GPU timing runs.
    pub fn frame_begin(&self) {
        if !self.is_enabled() && self.enable_next.load(Ordering::Relaxed) {
            self.enabled.store(true, Ordering::Relaxed);
        }
        if !self.is_gpu_enabled()
            && self.is_enabled()
            && self.gpu_enable_next.load(Ordering::Relaxed)
        {
            self.gpu_enabled.store(true, Ordering::Relaxed);
        } else if self.is_gpu_enabled() {
            let mut graphics = self.graphics();
            graphics.timer_idx = graphics.timer_idx.wrapping_add(1) % FRAME_BUFFER_SIZE;
        }
    }

    /// `source_profiler_frame_collect`: pushes each source's sample of the
    /// previous frame into its entry, then applies a pending disable.
    pub fn frame_collect<H: Host>(&self, host: &H) {
        if !self.is_enabled() {
            return;
        }
        host.collect_scope_begin();

        let gpu_enabled = self.is_gpu_enabled();
        let mut gpu = RangeData::default();
        let mut gpu_ready = false;
        if gpu_enabled {
            let range = {
                let graphics = self.graphics();
                let idx = graphics.timer_idx.wrapping_add(1) % FRAME_BUFFER_SIZE;
                graphics.timer_ranges.get(idx).copied().unwrap_or(0)
            };
            if range != 0 {
                gpu_ready = true;
                host.enter_graphics();
                gpu = host.range_get_data(range);
            }
            if gpu.disjoint {
                host.warn_disjoint();
            }
        }

        {
            let mut graphics = self.graphics();
            let mut entries = self.entries_write();
            let capacity = usize::try_from(self.samples_per_entry()).unwrap_or(usize::MAX);
            for (&source, samples) in &mut graphics.samples {
                let frame_idx = samples.frame_idx.wrapping_add(1) % FRAME_BUFFER_SIZE;
                let Some(sample) = samples.frames.get_mut(frame_idx) else {
                    continue;
                };
                if sample.tick == 0 {
                    continue;
                }
                let entry = entries
                    .entry(source)
                    .or_insert_with(|| Entry::new(capacity));
                collect_sample(
                    entry,
                    sample,
                    gpu_ready && !gpu.disjoint,
                    gpu.frequency,
                    host,
                );
                if host.is_async_video(source) {
                    entry.async_rendered_ts.push(host.last_async_ts(source));
                }
            }
        }

        if gpu_enabled && gpu_ready {
            host.leave_graphics();
        }

        if !self.enable_next.load(Ordering::Relaxed) {
            self.enabled.store(false, Ordering::Relaxed);
            self.gpu_enabled.store(false, Ordering::Relaxed);
            self.shutdown(host);
        } else if !self.gpu_enable_next.load(Ordering::Relaxed) {
            self.gpu_enabled.store(false, Ordering::Relaxed);
            self.reset_gpu_timers(host);
        }

        host.collect_scope_end();
    }

    /// `source_profiler_async_frame_received`.
    pub fn async_frame_received<H: Host>(&self, source: Handle, host: &H) {
        if !self.is_enabled() {
            return;
        }
        let ts = host.now_ns();
        if let Some(entry) = self.entries_write().get_mut(&source) {
            entry.async_frame_ts.push(ts);
        }
    }

    /// `source_profiler_source_tick_start`: the start time, 0 when off.
    pub fn source_tick_start<H: Host>(&self, host: &H) -> u64 {
        if !self.is_enabled() {
            return 0;
        }
        host.now_ns()
    }

    /// `source_profiler_source_tick_end`: a source's first tick creates its
    /// samples; later ones advance its frame first (a tick happens once, at
    /// the start of each frame).
    pub fn source_tick_end<H: Host>(&self, source: Handle, start: u64, host: &H) {
        if !self.is_enabled() {
            return;
        }
        let delta = host.now_ns().wrapping_sub(start);
        let mut graphics = self.graphics();
        let samples = match graphics.samples.entry(source) {
            std::collections::btree_map::Entry::Vacant(slot) => slot.insert(SourceSamples::new()),
            std::collections::btree_map::Entry::Occupied(slot) => {
                let samples = slot.into_mut();
                samples.frame_idx = samples.frame_idx.wrapping_add(1) % FRAME_BUFFER_SIZE;
                samples
            }
        };
        if let Some(frame) = samples.current() {
            frame.tick = delta;
        }
    }

    /// `source_profiler_source_render_begin`: `None` when off (the C
    /// returns 0 and leaves `*timer` alone), else the start time and the
    /// started GPU timer (0 without GPU timing).
    pub fn source_render_begin<H: Host>(&self, host: &H) -> Option<(u64, Handle)> {
        if !self.is_enabled() {
            return None;
        }
        let timer = if self.is_gpu_enabled() {
            let timer = host.timer_create();
            host.timer_begin(timer);
            timer
        } else {
            0
        };
        Some((host.now_ns(), timer))
    }

    /// `source_profiler_source_render_end`: records a render pass in the
    /// source's current frame. A timer of a source without samples is
    /// destroyed.
    pub fn source_render_end<H: Host>(&self, source: Handle, start: u64, timer: Handle, host: &H) {
        if !self.is_enabled() {
            return;
        }
        if timer != 0 {
            host.timer_end(timer);
        }
        let delta = host.now_ns().wrapping_sub(start);
        let kept = {
            let mut graphics = self.graphics();
            match graphics
                .samples
                .get_mut(&source)
                .and_then(SourceSamples::current)
            {
                Some(frame) => {
                    frame.render_cpu.push(delta);
                    if timer != 0 {
                        frame.render_timers.push(timer);
                    }
                    true
                }
                None => false,
            }
        };
        if !kept && timer != 0 {
            host.timer_destroy(timer);
        }
    }

    /// `source_profiler_remove_source`: whether to queue
    /// [`SourceProfiler::delete_source`] on the graphics thread.
    pub fn should_queue_removal(&self) -> bool {
        self.is_enabled()
    }

    /// `task_delete_source`: drops a source's samples and entry.
    pub fn delete_source<H: Host>(&self, source: Handle, host: &H) {
        let removed = self.graphics().samples.remove(&source);
        if let Some(mut samples) = removed {
            let mut timers = Vec::new();
            samples.take_timers(&mut timers);
            destroy_timers(&timers, host);
        }
        self.entries_write().remove(&source);
    }

    /// `source_profiler_fill_result`.
    pub fn fill_result<H: Host>(&self, source: Handle, host: &H) -> Fill {
        if !self.is_enabled() {
            return Fill::Disabled;
        }
        let entries = self.entries_read();
        let Some(entry) = entries.get(&source) else {
            return Fill::Missing;
        };
        let mut result = ProfilerResult::default();
        calculate_tick(entry.tick.values(), &mut result);
        let gpu = self
            .is_gpu_enabled()
            .then(|| (entry.render_gpu.values(), entry.render_gpu_sum.values()));
        calculate_render(
            entry.render_cpu.values(),
            entry.render_cpu_sum.values(),
            gpu,
            &mut result,
        );
        if host.is_async_video(source) {
            if let Some(fps) = calculate_fps(entry.async_frame_ts.values()) {
                result.async_input = fps.fps;
                result.async_input_best = fps.best;
                result.async_input_worst = fps.worst;
            }
            if let Some(fps) = calculate_fps(entry.async_rendered_ts.values()) {
                result.async_rendered = fps.fps;
                result.async_rendered_best = fps.best;
                result.async_rendered_worst = fps.worst;
            }
        }
        Fill::Found(result)
    }

    /// A copy of a source's entry, for tests and the differential check.
    pub fn entry(&self, source: Handle) -> Option<Entry> {
        self.entries_read().get(&source).cloned()
    }

    /// A copy of a source's samples, for tests and the differential check.
    pub fn samples(&self, source: Handle) -> Option<SourceSamples> {
        self.graphics().samples.get(&source).cloned()
    }

    /// The sources with samples, in key order.
    pub fn sampled_sources(&self) -> Vec<Handle> {
        self.graphics().samples.keys().copied().collect()
    }

    /// The sources with entries, in key order.
    pub fn entry_sources(&self) -> Vec<Handle> {
        self.entries_read().keys().copied().collect()
    }

    /// The current timer range slot and the ranges (`timer_idx`,
    /// `timer_ranges`).
    pub fn timer_ranges(&self) -> (usize, [Handle; FRAME_BUFFER_SIZE]) {
        let graphics = self.graphics();
        (graphics.timer_idx, graphics.timer_ranges)
    }
}

/// `frame_sample_destroy`'s timers, in one graphics context.
fn destroy_timers<H: Host>(timers: &[Handle], host: &H) {
    if timers.is_empty() {
        return;
    }
    host.enter_graphics();
    for &timer in timers {
        host.timer_destroy(timer);
    }
    host.leave_graphics();
}

/// One source's part of `source_profiler_frame_collect`, without the async
/// timestamp.
fn collect_sample<H: Host>(
    entry: &mut Entry,
    sample: &mut FrameSample,
    gpu_valid: bool,
    frequency: u64,
    host: &H,
) {
    entry.tick.push(sample.tick);

    if let Some(&first) = sample.render_cpu.first() {
        let sum = sample
            .render_cpu
            .iter()
            .fold(0u64, |acc, &v| acc.wrapping_add(v));
        entry.render_cpu.push(first);
        entry.render_cpu_sum.push(sum);
        sample.render_cpu.clear();
    } else {
        entry.render_cpu.push(0);
        entry.render_cpu_sum.push(0);
    }

    if sample.render_timers.is_empty() {
        entry.render_gpu.push(0);
        entry.render_gpu_sum.push(0);
        return;
    }
    let mut sum = 0u64;
    let mut first = 0u64;
    for timer in sample.render_timers.drain(..) {
        let ticks = if gpu_valid {
            host.timer_get_data(timer)
        } else {
            None
        };
        if let Some(ticks) = ticks {
            sum = sum.wrapping_add(ticks_to_ns(ticks, frequency));
            if first == 0 {
                first = sum;
            }
        }
        host.timer_destroy(timer);
    }
    if first != 0 {
        entry.render_gpu.push(first);
        entry.render_gpu_sum.push(sum);
    }
}

/// `util_mul_div64(ticks, 1000000000, frequency)`. A frequency of 0 gives
/// 0; the C divides by zero.
pub fn ticks_to_ns(ticks: u64, frequency: u64) -> u64 {
    if frequency == 0 {
        return 0;
    }
    mul_div64(ticks, 1_000_000_000, frequency)
}
