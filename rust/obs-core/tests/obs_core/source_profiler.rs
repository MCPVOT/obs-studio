//! Tier 1: the safe core of `libobs/util/source-profiler.c.h`.
//!
//! The first six tests mirror `test/cmocka/test_source_profiler.c` case by
//! case, with the same names. The cmocka test runs real video, so it can
//! only pin relations between real timings; here the clock is scripted, so
//! the same cases check exact values. The rest pins the parts the cmocka
//! test cannot reach: the ring buffer, the tick/render/fps math, the
//! one-frame delay of the sample ring, GPU timers and ranges, video reset,
//! and teardown.
//!
//! Characterized, not endorsed (kept as the C has it): a source that is not
//! ticked keeps its last tick and collection pushes it again; GPU timers
//! left after GPU timing stopped are destroyed outside the graphics context;
//! sums wrap; `gpu_enable(true)` while the profiler is not enabled is
//! dropped; a disjoint frame pushes no GPU sample at all.
//!
//! Intentional differences from the C (also in rust/README.md and #1886):
//! a zero-capacity ring drops pushes and a GPU frequency of 0 converts to
//! 0 ns (the C divides by zero); `delete_source` takes the entry write lock
//! (the C uses the read lock); the timers of dropped samples are destroyed
//! in one graphics context after the sample lock is released.

use std::cell::{Cell, RefCell};
use std::collections::{HashMap, HashSet, VecDeque};

use obs_core::source_profiler::{
    CircleBuf, FRAME_BUFFER_SIZE, Fill, FpsStats, Handle, Host, ProfilerResult, RangeData,
    SourceProfiler, average_max, calculate_fps, calculate_render, calculate_tick, ticks_to_ns,
};

const SYNC: Handle = 0x1000;
const ASYNC: Handle = 0x2000;
const QUIET: Handle = 0x3000;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
enum Call {
    Enter,
    Leave,
    TimerCreate(Handle),
    TimerBegin(Handle),
    TimerEnd(Handle),
    TimerGetData(Handle),
    TimerDestroy(Handle),
    RangeCreate(Handle),
    RangeBegin(Handle),
    RangeEnd(Handle),
    RangeGetData(Handle),
    RangeDestroy(Handle),
    WarnDisjoint,
    ScopeBegin,
    ScopeEnd,
}

/// A scripted libobs: the clock answers from a queue, GPU timers report
/// the ticks set before they were created, every call is logged.
struct Fake {
    times: RefCell<VecDeque<u64>>,
    next_handle: Cell<Handle>,
    next_ticks: Cell<Option<u64>>,
    timer_ticks: RefCell<HashMap<Handle, Option<u64>>>,
    range: Cell<RangeData>,
    null_handles: Cell<bool>,
    async_sources: RefCell<HashSet<Handle>>,
    last_async: RefCell<HashMap<Handle, u64>>,
    calls: RefCell<Vec<Call>>,
}

impl Fake {
    fn new() -> Self {
        Self {
            times: RefCell::new(VecDeque::new()),
            next_handle: Cell::new(0x9000),
            next_ticks: Cell::new(Some(0)),
            timer_ticks: RefCell::new(HashMap::new()),
            range: Cell::new(RangeData {
                disjoint: false,
                frequency: 1_000_000_000,
            }),
            null_handles: Cell::new(false),
            async_sources: RefCell::new(HashSet::from([ASYNC])),
            last_async: RefCell::new(HashMap::new()),
            calls: RefCell::new(Vec::new()),
        }
    }

    fn at(&self, times: &[u64]) {
        self.times.borrow_mut().extend(times.iter().copied());
    }

    fn handle(&self) -> Handle {
        if self.null_handles.get() {
            return 0;
        }
        let h = self.next_handle.get();
        self.next_handle.set(h + 0x10);
        h
    }

    fn log(&self, call: Call) {
        self.calls.borrow_mut().push(call);
    }

    fn take_calls(&self) -> Vec<Call> {
        std::mem::take(&mut self.calls.borrow_mut())
    }

    fn destroyed_timers(&self) -> Vec<Handle> {
        self.calls
            .borrow()
            .iter()
            .filter_map(|c| match c {
                Call::TimerDestroy(h) => Some(*h),
                _ => None,
            })
            .collect()
    }

    fn graphics_balanced(&self) -> bool {
        let calls = self.calls.borrow();
        let enters = calls.iter().filter(|c| **c == Call::Enter).count();
        let leaves = calls.iter().filter(|c| **c == Call::Leave).count();
        enters == leaves
    }
}

impl Host for Fake {
    fn now_ns(&self) -> u64 {
        self.times
            .borrow_mut()
            .pop_front()
            .expect("the test scripted every clock read")
    }
    fn enter_graphics(&self) {
        self.log(Call::Enter);
    }
    fn leave_graphics(&self) {
        self.log(Call::Leave);
    }
    fn timer_create(&self) -> Handle {
        let h = self.handle();
        self.timer_ticks
            .borrow_mut()
            .insert(h, self.next_ticks.get());
        self.log(Call::TimerCreate(h));
        h
    }
    fn timer_begin(&self, timer: Handle) {
        self.log(Call::TimerBegin(timer));
    }
    fn timer_end(&self, timer: Handle) {
        self.log(Call::TimerEnd(timer));
    }
    fn timer_get_data(&self, timer: Handle) -> Option<u64> {
        self.log(Call::TimerGetData(timer));
        self.timer_ticks.borrow().get(&timer).copied().flatten()
    }
    fn timer_destroy(&self, timer: Handle) {
        self.log(Call::TimerDestroy(timer));
    }
    fn range_create(&self) -> Handle {
        let h = self.handle();
        self.log(Call::RangeCreate(h));
        h
    }
    fn range_begin(&self, range: Handle) {
        self.log(Call::RangeBegin(range));
    }
    fn range_end(&self, range: Handle) {
        self.log(Call::RangeEnd(range));
    }
    fn range_get_data(&self, range: Handle) -> RangeData {
        self.log(Call::RangeGetData(range));
        self.range.get()
    }
    fn range_destroy(&self, range: Handle) {
        self.log(Call::RangeDestroy(range));
    }
    fn is_async_video(&self, source: Handle) -> bool {
        self.async_sources.borrow().contains(&source)
    }
    fn last_async_ts(&self, source: Handle) -> u64 {
        self.last_async.borrow().get(&source).copied().unwrap_or(0)
    }
    fn warn_disjoint(&self) {
        self.log(Call::WarnDisjoint);
    }
    fn collect_scope_begin(&self) {
        self.log(Call::ScopeBegin);
    }
    fn collect_scope_end(&self) {
        self.log(Call::ScopeEnd);
    }
}

/// A profiler after `obs_reset_video` at 30 fps (150 samples per ring).
fn profiler(host: &Fake) -> SourceProfiler {
    let p = SourceProfiler::new();
    p.reset_video(30, 1, host);
    host.take_calls();
    p
}

fn tick(p: &SourceProfiler, host: &Fake, source: Handle, start: u64, delta: u64) {
    host.at(&[start, start + delta]);
    let s = p.source_tick_start(host);
    p.source_tick_end(source, s, host);
}

/// One render pass; returns the GPU timer (0 without GPU timing).
fn render(p: &SourceProfiler, host: &Fake, source: Handle, start: u64, delta: u64) -> Handle {
    host.at(&[start, start + delta]);
    let (s, timer) = p.source_render_begin(host).expect("profiler enabled");
    p.source_render_end(source, s, timer, host);
    timer
}

/// A frame as `obs_graphics_thread_loop` runs it: begin, each source's tick
/// (`tick` ns) and render passes (`renders`), collect.
fn frame(p: &SourceProfiler, host: &Fake, sources: &[(Handle, u64, &[u64])]) {
    p.frame_begin();
    for &(source, t, _) in sources {
        tick(p, host, source, 100, t);
    }
    p.render_begin(host);
    for &(source, _, renders) in sources {
        for &r in renders {
            render(p, host, source, 1000, r);
        }
    }
    p.render_end(host);
    p.frame_collect(host);
}

fn found(fill: Fill) -> ProfilerResult {
    match fill {
        Fill::Found(r) => r,
        other => panic!("expected an entry, got {other:?}"),
    }
}

fn assert_cpu_relations(r: &ProfilerResult) {
    assert!(r.tick_max >= r.tick_avg);
    assert!(r.render_max >= r.render_avg);
    assert!(r.render_sum >= r.render_avg);
}

fn assert_gpu_zero(r: &ProfilerResult) {
    assert_eq!(
        (r.render_gpu_avg, r.render_gpu_max, r.render_gpu_sum),
        (0, 0, 0)
    );
}

fn assert_async_zero(r: &ProfilerResult) {
    assert_eq!(r.async_input, 0.0);
    assert_eq!(r.async_rendered, 0.0);
    assert_eq!(
        (
            r.async_input_best,
            r.async_input_worst,
            r.async_rendered_best,
            r.async_rendered_worst
        ),
        (0, 0, 0, 0)
    );
}

// ---------------------------------------------------------------------------
// test_source_profiler.c, case by case

/// `disabled_reports_nothing`.
#[test]
fn disabled_reports_nothing() {
    let host = Fake::new();
    let p = profiler(&host);
    assert_eq!(p.fill_result(0, &host), Fill::Disabled);

    // frames run, nothing is collected and nothing reads the clock
    for _ in 0..3 {
        p.frame_begin();
        assert_eq!(p.source_tick_start(&host), 0);
        p.source_tick_end(SYNC, 0, &host);
        assert_eq!(p.source_render_begin(&host), None);
        p.source_render_end(SYNC, 0, 0, &host);
        p.render_begin(&host);
        p.render_end(&host);
        p.frame_collect(&host);
    }
    assert_eq!(p.fill_result(SYNC, &host), Fill::Disabled);
    assert!(p.sampled_sources().is_empty());
    assert!(host.take_calls().is_empty());
}

/// `cpu_profile_fills_result`.
#[test]
fn cpu_profile_fills_result() {
    let host = Fake::new();
    let p = profiler(&host);
    p.gpu_enable(true);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20, 5])]);
    frame(&p, &host, &[(SYNC, 30, &[40])]);
    frame(&p, &host, &[(SYNC, 50, &[60])]);
    assert!(!p.is_gpu_enabled());

    // two frames collected: (10; 20+5) and (30; 40)
    let r = found(p.fill_result(SYNC, &host));
    assert_eq!((r.tick_avg, r.tick_max), (20, 30));
    assert_eq!((r.render_avg, r.render_max, r.render_sum), (30, 40, 32));
    assert!(r.tick_max > 0);
    assert_cpu_relations(&r);
    assert_gpu_zero(&r);
    assert_async_zero(&r);
    // no GPU timer was ever made
    assert!(
        !host
            .take_calls()
            .iter()
            .any(|c| matches!(c, Call::TimerCreate(_) | Call::RangeCreate(_)))
    );
}

/// `unknown_source_zeroes_result`.
#[test]
fn unknown_source_zeroes_result() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20])]);
    frame(&p, &host, &[(SYNC, 10, &[20])]);
    assert_eq!(p.fill_result(0xdead, &host), Fill::Missing);
    assert_eq!(p.fill_result(0, &host), Fill::Missing);
    assert!(matches!(p.fill_result(SYNC, &host), Fill::Found(_)));
}

/// `async_source_reports_fps`.
#[test]
fn async_source_reports_fps() {
    let host = Fake::new();
    let p = profiler(&host);
    host.last_async.borrow_mut().insert(ASYNC, 500);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);

    let r = found(p.fill_result(ASYNC, &host));
    assert_eq!(r.async_input, 0.0);
    assert_eq!((r.async_input_best, r.async_input_worst), (0, 0));
    // no render pass: the CPU rings hold zeros
    assert_eq!((r.render_avg, r.render_max, r.render_sum), (0, 0, 0));

    // three frames: deltas 30 ms and 40 ms
    host.at(&[1_000_000_000, 1_030_000_000, 1_070_000_000]);
    for _ in 0..3 {
        p.async_frame_received(ASYNC, &host);
    }
    let r = found(p.fill_result(ASYNC, &host));
    assert_eq!(r.async_input, 1.0e9 / 35_000_000.0);
    assert_eq!(
        (r.async_input_best, r.async_input_worst),
        (30_000_000, 40_000_000)
    );
    // rendered: one timestamp so far (500), so no fps
    assert_eq!(
        p.entry(ASYNC).expect("entry").async_rendered_ts.values(),
        &[500]
    );
    assert_eq!(r.async_rendered, 0.0);
    assert_eq!((r.async_rendered_best, r.async_rendered_worst), (0, 0));
    assert_gpu_zero(&r);

    // a second, later rendered timestamp gives a rendered fps
    host.last_async.borrow_mut().insert(ASYNC, 600);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    let r = found(p.fill_result(ASYNC, &host));
    assert_eq!(r.async_rendered, 1.0e9 / 100.0);
    assert_eq!((r.async_rendered_best, r.async_rendered_worst), (100, 100));

    // the sync source never reports fps
    assert_async_zero(&found(p.fill_result(SYNC, &host)));
    // frames for a source without an entry are not kept, and read the clock
    host.at(&[1]);
    p.async_frame_received(0xdead, &host);
    assert!(host.times.borrow().is_empty());
}

/// `removed_source_is_forgotten`.
#[test]
fn removed_source_is_forgotten() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20]), (QUIET, 5, &[])]);
    frame(&p, &host, &[(SYNC, 10, &[20]), (QUIET, 5, &[])]);
    assert_cpu_relations(&found(p.fill_result(QUIET, &host)));

    assert!(p.should_queue_removal());
    p.delete_source(QUIET, &host);
    assert_eq!(p.fill_result(QUIET, &host), Fill::Missing);
    assert!(p.samples(QUIET).is_none());
    frame(&p, &host, &[(SYNC, 10, &[20])]);
    assert_eq!(p.fill_result(QUIET, &host), Fill::Missing);
    assert!(matches!(p.fill_result(SYNC, &host), Fill::Found(_)));

    // disabled: nothing to queue
    p.enable(false);
    frame(&p, &host, &[]);
    assert!(!p.should_queue_removal());
}

/// `disable_drops_entries`.
#[test]
fn disable_drops_entries() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    host.at(&[10, 20]);
    p.async_frame_received(ASYNC, &host);
    p.async_frame_received(ASYNC, &host);

    // the disable applies at the end of the next collect
    p.enable(false);
    assert!(p.is_enabled());
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    assert!(!p.is_enabled());
    assert_eq!(p.fill_result(SYNC, &host), Fill::Disabled);
    assert!(p.sampled_sources().is_empty());
    assert!(p.entry_sources().is_empty());

    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    frame(&p, &host, &[(SYNC, 10, &[20]), (ASYNC, 1, &[])]);
    let r = found(p.fill_result(SYNC, &host));
    assert_cpu_relations(&r);
    assert_gpu_zero(&r);
    // the async input timestamps went with the old entry
    assert_eq!(found(p.fill_result(ASYNC, &host)).async_input, 0.0);
    assert_eq!(p.entry(SYNC).expect("entry").tick.values(), &[10]);
}

// ---------------------------------------------------------------------------
// ucirclebuf

#[test]
fn circlebuf_fills_then_overwrites_oldest() {
    let mut b = CircleBuf::new(3);
    assert!(b.is_empty());
    assert_eq!(b.capacity(), 3);
    for v in [1, 2, 3] {
        b.push(v);
    }
    assert_eq!((b.values(), b.idx(), b.len()), (&[1, 2, 3][..], 3, 3));
    b.push(4);
    assert_eq!((b.values(), b.idx()), (&[4, 2, 3][..], 1));
    b.push(5);
    b.push(6);
    assert_eq!((b.values(), b.idx()), (&[4, 5, 6][..], 3));
    b.push(7);
    assert_eq!((b.values(), b.idx()), (&[7, 5, 6][..], 1));
}

#[test]
fn circlebuf_of_one() {
    let mut b = CircleBuf::new(1);
    b.push(9);
    b.push(8);
    assert_eq!((b.values(), b.idx()), (&[8][..], 1));
}

/// Intentional difference: the C divides by zero.
#[test]
fn circlebuf_of_zero_drops_pushes() {
    let mut b = CircleBuf::new(0);
    b.push(1);
    assert!(b.is_empty());
    assert_eq!(b.idx(), 0);
}

// ---------------------------------------------------------------------------
// calculate_tick / calculate_render / calculate_fps

#[test]
fn tick_average_truncates_and_keeps_max() {
    let mut r = ProfilerResult::default();
    calculate_tick(&[1, 2, 4], &mut r);
    assert_eq!((r.tick_avg, r.tick_max), (2, 4));
    assert_eq!(average_max(&[]), None);

    // no values: the result keeps what it had
    let mut r = ProfilerResult {
        tick_avg: 7,
        tick_max: 8,
        ..ProfilerResult::default()
    };
    calculate_tick(&[], &mut r);
    assert_eq!((r.tick_avg, r.tick_max), (7, 8));
}

/// Characterized, not endorsed: the sum wraps like `uint64_t`.
#[test]
fn tick_sum_wraps() {
    assert_eq!(average_max(&[u64::MAX, 2]), Some((0, u64::MAX)));
}

#[test]
fn render_cpu_and_gpu() {
    let mut r = ProfilerResult::default();
    calculate_render(&[10, 30], &[15, 45], None, &mut r);
    assert_eq!((r.render_avg, r.render_max, r.render_sum), (20, 30, 30));
    assert_gpu_zero(&r);

    calculate_render(
        &[10, 30],
        &[15, 45],
        Some((&[3, 9, 6], &[4, 10, 7])),
        &mut r,
    );
    assert_eq!(
        (r.render_gpu_avg, r.render_gpu_max, r.render_gpu_sum),
        (6, 9, 7)
    );
    // GPU on, but no GPU samples: zero
    let mut r = ProfilerResult::default();
    calculate_render(&[], &[], Some((&[], &[])), &mut r);
    assert_eq!(r, ProfilerResult::default());
}

#[test]
fn fps_first_delta_wraps_to_the_last_stored() {
    // storage order after the ring wrapped: [40, 50, 20, 30]
    let s = calculate_fps(&[40, 50, 20, 30]).expect("fps");
    // 40-30, 50-40, (20 <= 50 skipped), 30-20
    assert_eq!(
        s,
        FpsStats {
            fps: 1.0e9 / 10.0,
            best: 10,
            worst: 10
        }
    );
    let s = calculate_fps(&[10, 25, 45]).expect("fps");
    assert_eq!((s.best, s.worst), (15, 20));
    assert_eq!(s.fps, 1.0e9 / 17.5);
}

#[test]
fn fps_stops_at_zero_and_needs_a_delta() {
    assert_eq!(calculate_fps(&[]), None);
    assert_eq!(calculate_fps(&[5]), None);
    assert_eq!(calculate_fps(&[5, 5, 5]), None);
    assert_eq!(calculate_fps(&[0, 10, 20]), None);
    // the 0 ends the walk, 30 is never reached
    let s = calculate_fps(&[10, 20, 0, 30]).expect("fps");
    assert_eq!((s.best, s.worst), (10, 10));
    // a zero before a timestamp skips it
    assert_eq!(calculate_fps(&[10, 0]), None);
}

#[test]
fn ticks_convert_at_the_timer_frequency() {
    assert_eq!(ticks_to_ns(5, 1_000_000_000), 5);
    assert_eq!(ticks_to_ns(3, 1_000), 3_000_000);
    assert_eq!(ticks_to_ns(10_000_000, 10_000_000), 1_000_000_000);
    // intentional difference: the C divides by zero
    assert_eq!(ticks_to_ns(5, 0), 0);
}

// ---------------------------------------------------------------------------
// samples

#[test]
fn reset_video_sizes_rings_to_five_seconds() {
    let host = Fake::new();
    let p = SourceProfiler::new();
    for (num, den, samples) in [(30, 1, 150), (30000, 1001, 150), (60, 1, 300), (1, 2, 5)] {
        p.reset_video(num, den, &host);
        assert_eq!(p.samples_per_entry(), samples, "{num}/{den}");
    }
    p.reset_video(24, 1, &host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 1, &[])]);
    frame(&p, &host, &[(SYNC, 1, &[])]);
    assert_eq!(p.entry(SYNC).expect("entry").tick.capacity(), 120);

    // a reset drops everything
    p.reset_video(24, 1, &host);
    assert!(p.sampled_sources().is_empty());
    assert!(p.entry_sources().is_empty());
}

#[test]
fn samples_are_collected_one_frame_late() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    // the enable applies at frame begin
    assert!(!p.is_enabled());
    p.frame_begin();
    assert!(p.is_enabled());

    tick(&p, &host, SYNC, 100, 7);
    let s = p.samples(SYNC).expect("samples");
    assert_eq!((s.frame_idx, s.frames[0].tick, s.frames[1].tick), (0, 7, 0));
    render(&p, &host, SYNC, 1000, 3);
    render(&p, &host, SYNC, 1000, 4);
    p.frame_collect(&host);
    // frame 1 has no data yet: no entry
    assert!(p.entry(SYNC).is_none());

    p.frame_begin();
    tick(&p, &host, SYNC, 100, 8);
    assert_eq!(p.samples(SYNC).expect("samples").frame_idx, 1);
    p.frame_collect(&host);
    let e = p.entry(SYNC).expect("entry");
    assert_eq!(e.tick.values(), &[7]);
    assert_eq!(e.render_cpu.values(), &[3]);
    assert_eq!(e.render_cpu_sum.values(), &[7]);
    assert_eq!(e.render_gpu.values(), &[0]);
    assert_eq!(e.render_gpu_sum.values(), &[0]);
    assert!(e.async_frame_ts.is_empty());
    assert!(e.async_rendered_ts.is_empty());
    // the collected frame's passes are cleared, its tick is kept
    let s = p.samples(SYNC).expect("samples");
    assert!(s.frames[0].render_cpu.is_empty());
    assert_eq!(s.frames[0].tick, 7);
    assert_eq!(FRAME_BUFFER_SIZE, 2);
}

/// Characterized, not endorsed: a source not ticked in a frame keeps its
/// tick in the ring, and collection pushes it again.
#[test]
fn untick_source_repeats_its_stale_tick() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 4, &[]), (QUIET, 9, &[])]);
    frame(&p, &host, &[(SYNC, 5, &[]), (QUIET, 11, &[])]);
    frame(&p, &host, &[(SYNC, 6, &[])]);
    frame(&p, &host, &[(SYNC, 7, &[])]);
    // QUIET: frames[0] = 9 and frames[1] = 11, its index stays at 1
    assert_eq!(p.entry(QUIET).expect("entry").tick.values(), &[9, 9, 9]);
    assert_eq!(p.entry(SYNC).expect("entry").tick.values(), &[4, 5, 6]);
}

#[test]
fn render_without_samples_destroys_its_timer() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    p.frame_begin();
    assert!(p.is_gpu_enabled());
    host.take_calls();
    let timer = render(&p, &host, SYNC, 1000, 3);
    assert_ne!(timer, 0);
    assert_eq!(
        host.take_calls(),
        vec![
            Call::TimerCreate(timer),
            Call::TimerBegin(timer),
            Call::TimerEnd(timer),
            Call::TimerDestroy(timer)
        ]
    );
    assert!(p.samples(SYNC).is_none());
}

#[test]
fn render_with_null_timer_keeps_only_cpu_time() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    host.null_handles.set(true);
    p.frame_begin();
    tick(&p, &host, SYNC, 100, 1);
    host.take_calls();
    assert_eq!(render(&p, &host, SYNC, 1000, 3), 0);
    // gs_timer_begin(NULL) is still called, gs_timer_end(NULL) is not
    assert_eq!(
        host.take_calls(),
        vec![Call::TimerCreate(0), Call::TimerBegin(0)]
    );
    let s = p.samples(SYNC).expect("samples");
    assert_eq!(s.frames[0].render_cpu, vec![3]);
    assert!(s.frames[0].render_timers.is_empty());
}

// ---------------------------------------------------------------------------
// GPU timing

#[test]
fn gpu_enable_needs_a_pending_enable() {
    let host = Fake::new();
    let p = profiler(&host);
    p.gpu_enable(true);
    p.enable(true);
    p.frame_begin();
    assert!(p.is_enabled());
    assert!(!p.is_gpu_enabled());

    p.gpu_enable(true);
    p.frame_begin();
    assert!(p.is_gpu_enabled());
}

#[test]
fn gpu_ranges_rotate_and_are_read_one_frame_late() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    host.range.set(RangeData {
        disjoint: false,
        frequency: 1_000,
    });

    // frame A: GPU timing turns on, timer_idx stays 0
    host.next_ticks.set(Some(2));
    frame(&p, &host, &[(SYNC, 10, &[20, 5])]);
    let (idx, ranges) = p.timer_ranges();
    assert_eq!(idx, 0);
    assert_ne!(ranges[0], 0);
    assert_eq!(ranges[1], 0);
    // range 1 does not exist yet: not ready, no context entered to read it
    let calls = host.take_calls();
    assert!(!calls.iter().any(|c| matches!(c, Call::RangeGetData(_))));

    // frame B: timer_idx 1; collect reads range 0 and frame A's timers
    host.next_ticks.set(Some(4));
    frame(&p, &host, &[(SYNC, 10, &[20])]);
    let (idx, ranges_b) = p.timer_ranges();
    assert_eq!(idx, 1);
    assert_eq!(ranges_b[0], ranges[0]);
    assert_ne!(ranges_b[1], 0);
    let calls = host.take_calls();
    assert!(calls.contains(&Call::RangeGetData(ranges[0])));
    assert!(host.graphics_balanced());
    let e = p.entry(SYNC).expect("entry");
    // 2 ticks at 1 kHz = 2 ms per pass; first pass 2 ms, sum 4 ms
    assert_eq!(e.render_gpu.values(), &[2_000_000]);
    assert_eq!(e.render_gpu_sum.values(), &[4_000_000]);

    frame(&p, &host, &[(SYNC, 10, &[20])]);
    let r = found(p.fill_result(SYNC, &host));
    assert_eq!(
        (r.render_gpu_avg, r.render_gpu_max, r.render_gpu_sum),
        (3_000_000, 4_000_000, 4_000_000)
    );
}

#[test]
fn gpu_timer_without_data_is_skipped() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    frame(&p, &host, &[(SYNC, 10, &[])]);
    // first pass has no data, second does: first is the sum so far
    p.frame_begin();
    tick(&p, &host, SYNC, 100, 10);
    p.render_begin(&host);
    host.next_ticks.set(None);
    render(&p, &host, SYNC, 0, 1);
    host.next_ticks.set(Some(7));
    render(&p, &host, SYNC, 0, 1);
    p.render_end(&host);
    p.frame_collect(&host);
    frame(&p, &host, &[(SYNC, 10, &[])]);
    let e = p.entry(SYNC).expect("entry");
    // frame A had no pass (zeros), frame B: first = 7, sum = 7
    assert_eq!(e.render_gpu.values(), &[0, 7]);
    assert_eq!(e.render_gpu_sum.values(), &[0, 7]);
}

/// Characterized, not endorsed: a disjoint frame warns and pushes no GPU
/// sample at all (not even a zero), while its timers are destroyed.
#[test]
fn disjoint_frame_pushes_no_gpu_sample() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    host.next_ticks.set(Some(5));
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    host.range.set(RangeData {
        disjoint: true,
        frequency: 1_000_000_000,
    });
    host.take_calls();
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    let calls = host.take_calls();
    assert!(calls.contains(&Call::WarnDisjoint));
    assert!(!calls.iter().any(|c| matches!(c, Call::TimerGetData(_))));
    assert_eq!(
        calls
            .iter()
            .filter(|c| matches!(c, Call::TimerDestroy(_)))
            .count(),
        1
    );
    let e = p.entry(SYNC).expect("entry");
    assert!(e.render_gpu.is_empty());
    assert_eq!(e.render_cpu.values(), &[1]);
}

#[test]
fn gpu_disable_destroys_ranges_and_leftover_timers() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    let (_, ranges) = p.timer_ranges();

    p.gpu_enable(false);
    host.take_calls();
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    assert!(!p.is_gpu_enabled());
    let calls = host.take_calls();
    for range in ranges {
        assert!(calls.contains(&Call::RangeDestroy(range)));
    }
    assert_eq!(p.timer_ranges().1, [0, 0]);

    // the timer of the last GPU frame is destroyed at the next collect,
    // outside a graphics context, and no GPU sample is pushed for it.
    // Characterized, not endorsed: while GPU timing stays off, every
    // collect enters the context again to reset the (empty) ranges.
    let before = p.entry(SYNC).expect("entry").render_gpu.len();
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    let calls = host.take_calls();
    assert!(
        matches!(
            calls.as_slice(),
            [
                Call::ScopeBegin,
                Call::TimerDestroy(_),
                Call::Enter,
                Call::Leave,
                Call::ScopeEnd
            ]
        ),
        "{calls:?}"
    );
    assert_eq!(p.entry(SYNC).expect("entry").render_gpu.len(), before);
}

#[test]
fn disable_destroys_pending_timers_and_ranges() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    p.frame_begin();
    tick(&p, &host, SYNC, 100, 1);
    let pending = render(&p, &host, SYNC, 0, 1);
    p.enable(false);
    host.take_calls();
    p.frame_collect(&host);
    assert!(!p.is_enabled() && !p.is_gpu_enabled());
    assert!(host.destroyed_timers().contains(&pending));
    assert!(host.graphics_balanced());
    assert_eq!(p.timer_ranges().1, [0, 0]);
    assert!(p.sampled_sources().is_empty());
}

#[test]
fn delete_source_destroys_its_timers_in_context() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    p.gpu_enable(true);
    frame(&p, &host, &[(SYNC, 10, &[])]);
    p.frame_begin();
    tick(&p, &host, SYNC, 100, 1);
    let pending = render(&p, &host, SYNC, 0, 1);
    host.take_calls();
    p.delete_source(SYNC, &host);
    assert_eq!(
        host.take_calls(),
        vec![Call::Enter, Call::TimerDestroy(pending), Call::Leave]
    );
    assert!(p.entry(SYNC).is_none());
    // deleting what is not there does nothing
    p.delete_source(SYNC, &host);
    assert!(host.take_calls().is_empty());
}

#[test]
fn collect_is_one_profiler_scope() {
    let host = Fake::new();
    let p = profiler(&host);
    p.enable(true);
    frame(&p, &host, &[(SYNC, 10, &[1])]);
    let calls = host.take_calls();
    assert_eq!(calls.first(), Some(&Call::ScopeBegin));
    assert_eq!(calls.last(), Some(&Call::ScopeEnd));
}

#[test]
fn profiler_is_shareable_across_threads() {
    fn assert_sync<T: Sync + Send>() {}
    assert_sync::<SourceProfiler>();
}
