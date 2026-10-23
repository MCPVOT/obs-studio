//! Tier 3: the Rust `media_remux_job_*` against the original C
//! (`obs_c_oracle::media_remux`). Both run over the same recording FFmpeg
//! stand-in (oracle/media_remux_ffmpeg.c): the C calls it directly, the
//! Rust shim through the real adapter `media-io/media-remux-ffmpeg.c.h`.
//! Each script is run through both, and the results, the progress reports,
//! and the log of every FFmpeg call and `blog` line, in order, must match.
//!
//! Intentional differences, excluded from the scripts: a NULL output name
//! (C: `strcmp(NULL)`); a packet whose stream index has no stream (C: reads
//! out of bounds). Not compared: `*job` after a failed create, which the C
//! leaves pointing at the job it freed and the Rust sets to NULL (checked in
//! `failed_create_clears_the_job`). The input's size: the C `stat`s it, the Rust uses
//! `os_get_file_size`; both read the same file here.

use core::ffi::{c_char, c_void};
use std::ffi::CString;
use std::sync::Mutex;

use obs_c_oracle::media_remux::{
    OracleRational as Rational, OracleRemuxPacket, OracleRemuxScript, OracleRemuxStream,
    oracle_media_remux_job_create, oracle_media_remux_job_destroy, oracle_media_remux_job_process,
    oracle_remux_ffmpeg_live, oracle_remux_ffmpeg_log, oracle_remux_ffmpeg_reset,
};
use obs_media_io::ffi::media_remux::{
    media_remux_job_create, media_remux_job_destroy, media_remux_job_process, media_remux_job_t,
};
use obs_media_io::media_remux::{
    CODEC_ID_HEVC, ERROR_EINVAL, ERROR_EOF, ERROR_INVALIDDATA, FMT_NOFILE, MEDIA_TYPE_AUDIO,
    NOPTS_VALUE,
};
use proptest::prelude::*;
use proptest::test_runner::{Config, TestRunner};

/// The stand-in is one global script and log.
static FFMPEG: Mutex<()> = Mutex::new(());

const AAC: i32 = 86018;
const H264: i32 = 27;

struct Progress {
    reports: Vec<f32>,
    stop_at: usize,
}

unsafe extern "C" fn on_progress(data: *mut c_void, percent: f32) -> bool {
    // SAFETY: a `Progress` for the duration of the process call.
    let progress = unsafe { &mut *data.cast::<Progress>() };
    progress.reports.push(percent);
    progress.stop_at == 0 || progress.reports.len() < progress.stop_at
}

type Create = unsafe extern "C" fn(*mut *mut c_void, *const c_char, *const c_char) -> bool;
type Process = unsafe extern "C" fn(
    *mut c_void,
    Option<unsafe extern "C" fn(*mut c_void, f32) -> bool>,
    *mut c_void,
) -> bool;
type Destroy = unsafe extern "C" fn(*mut c_void);

#[derive(Debug, PartialEq)]
struct Outcome {
    created: bool,
    job_null: bool,
    processed: Option<bool>,
    reports: Vec<f32>,
    log: String,
    live: usize,
}

#[derive(Debug, Clone)]
struct Case {
    script: OracleRemuxScript,
    /// The input file's size on disk; `None` for no input file.
    input_size: Option<usize>,
    out_name: &'static str,
    callback: bool,
    stop_at: usize,
}

fn run_one(
    case: &Case,
    input: &CString,
    output: &CString,
    create: Create,
    process: Process,
    destroy: Destroy,
) -> Outcome {
    // SAFETY: a plain script copy.
    unsafe { oracle_remux_ffmpeg_reset(&case.script) };
    let mut job: *mut c_void = core::ptr::null_mut();
    // SAFETY: valid pointers and strings.
    let created = unsafe { create(&mut job, input.as_ptr(), output.as_ptr()) };
    // Compared only for a created job: see the module docs.
    let job_null = created && job.is_null();
    let mut progress = Progress {
        reports: Vec::new(),
        stop_at: case.stop_at,
    };
    let processed = created.then(|| {
        let (callback, data) = if case.callback {
            (
                Some(on_progress as unsafe extern "C" fn(*mut c_void, f32) -> bool),
                (&raw mut progress).cast::<c_void>(),
            )
        } else {
            (None, core::ptr::null_mut())
        };
        // SAFETY: the job just created; `progress` outlives the call.
        unsafe { process(job, callback, data) }
    });
    // A failed create leaves the C's job pointer dangling (see the module
    // docs), so only a created job is destroyed. Destroying NULL does
    // nothing on both sides.
    if created {
        // SAFETY: the job from create.
        unsafe { destroy(job) };
    }
    // SAFETY: NULL is ignored.
    unsafe { destroy(core::ptr::null_mut()) };

    let mut buf = vec![0u8; 1 << 16];
    // SAFETY: a buffer of the size passed.
    let len = unsafe { oracle_remux_ffmpeg_log(buf.as_mut_ptr().cast(), buf.len()) };
    buf.truncate(len.min(buf.len()));
    Outcome {
        created,
        job_null,
        processed,
        reports: progress.reports,
        log: String::from_utf8_lossy(&buf).into_owned(),
        // SAFETY: a plain getter.
        live: unsafe { oracle_remux_ffmpeg_live() },
    }
}

unsafe extern "C" fn rust_create(
    job: *mut *mut c_void,
    input: *const c_char,
    output: *const c_char,
) -> bool {
    // SAFETY: forwarded.
    unsafe { media_remux_job_create(job.cast::<media_remux_job_t>(), input, output) }
}

unsafe extern "C" fn rust_process(
    job: *mut c_void,
    callback: Option<unsafe extern "C" fn(*mut c_void, f32) -> bool>,
    data: *mut c_void,
) -> bool {
    // SAFETY: forwarded.
    unsafe { media_remux_job_process(job.cast(), callback, data) }
}

unsafe extern "C" fn rust_destroy(job: *mut c_void) {
    // SAFETY: forwarded.
    unsafe { media_remux_job_destroy(job.cast()) }
}

fn compare(case: &Case) {
    let _guard = FFMPEG
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    let dir = std::env::temp_dir().join(format!("obs-media-remux-parity-{}", std::process::id()));
    std::fs::create_dir_all(&dir).unwrap();
    let input_path = dir.join("in.mkv");
    match case.input_size {
        Some(size) => std::fs::write(&input_path, vec![0u8; size]).unwrap(),
        None => {
            let _ = std::fs::remove_file(&input_path);
        }
    }
    let input = CString::new(input_path.to_str().unwrap()).unwrap();
    let output = if case.out_name == "SAME" {
        input.clone()
    } else {
        CString::new(dir.join(case.out_name).to_str().unwrap()).unwrap()
    };

    let c = run_one(
        case,
        &input,
        &output,
        oracle_media_remux_job_create,
        oracle_media_remux_job_process,
        oracle_media_remux_job_destroy,
    );
    let rust = run_one(
        case,
        &input,
        &output,
        rust_create,
        rust_process,
        rust_destroy,
    );
    let _ = std::fs::remove_file(&input_path);

    assert_eq!(c.live, 0, "the C left FFmpeg objects: {c:#?}");
    assert_eq!(
        rust.created, c.created,
        "create of {case:#?}\nC log:\n{}\nRust log:\n{}",
        c.log, rust.log
    );
    if c.log != rust.log {
        for (i, (a, b)) in c.log.lines().zip(rust.log.lines()).enumerate() {
            assert_eq!(b, a, "line {i} of the call log of {case:#?}");
        }
        assert_eq!(
            rust.log.lines().count(),
            c.log.lines().count(),
            "call log of {case:#?}"
        );
    }
    assert_eq!(rust, c, "{case:#?}");
}

fn stream(
    codec_type: i32,
    codec_id: i32,
    rate: i32,
    frame_size: i32,
    channels: i32,
) -> OracleRemuxStream {
    OracleRemuxStream {
        codec_type,
        codec_id,
        sample_rate: rate,
        frame_size,
        channels,
        in_time_base: Rational { num: 1, den: 1000 },
        out_time_base: Rational {
            num: 1,
            den: if rate > 0 { rate } else { 90000 },
        },
        frame_duration: frame_size,
    }
}

fn base_script() -> OracleRemuxScript {
    OracleRemuxScript {
        alloc_ok: true,
        tag_codec_id: CODEC_ID_HEVC,
        has_codec_tag: true,
        new_stream_fail_at: u32::MAX,
        copy_fail_at: u32::MAX,
        read_end_ret: ERROR_EOF,
        ..OracleRemuxScript::default()
    }
}

fn aac_case(rate: i32, frames: i64, gap: i64) -> Case {
    let mut script = base_script();
    script.nb_streams = 1;
    script.streams[0] = stream(MEDIA_TYPE_AUDIO, AAC, rate, 1024, 2);
    let mut n = 0;
    for i in 0..frames {
        if (40..40 + gap).contains(&i) {
            continue;
        }
        let start = (i * 1024 * 1000 + i64::from(rate) / 2) / i64::from(rate);
        let end = ((i + 1) * 1024 * 1000 + i64::from(rate) / 2) / i64::from(rate);
        script.packets[n] = OracleRemuxPacket {
            stream_index: 0,
            size: 300,
            pts: start,
            dts: start,
            duration: end - start,
            pos: 4096 + n as i64 * 300,
            write_ret: 0,
        };
        n += 1;
    }
    script.nb_packets = n as u32;
    Case {
        script,
        input_size: Some(4096 + n * 300),
        out_name: "out.mp4",
        callback: true,
        stop_at: 0,
    }
}

// ---------------------------------------------------------------------------
// chosen cases

#[test]
fn failed_create_clears_the_job() {
    let _guard = FFMPEG
        .lock()
        .unwrap_or_else(std::sync::PoisonError::into_inner);
    let mut case = aac_case(48000, 2, 0);
    case.script.alloc_ok = false;
    // SAFETY: a plain script copy.
    unsafe { oracle_remux_ffmpeg_reset(&case.script) };
    let input =
        std::env::temp_dir().join(format!("obs-media-remux-clear-{}.mkv", std::process::id()));
    std::fs::write(&input, b"x").unwrap();
    let input_c = CString::new(input.to_str().unwrap()).unwrap();
    let mut job: media_remux_job_t = 1 as media_remux_job_t;
    // SAFETY: valid pointers and strings.
    let created =
        unsafe { media_remux_job_create(&mut job, input_c.as_ptr(), c"out.nope".as_ptr()) };
    let _ = std::fs::remove_file(&input);
    assert!(!created);
    assert!(job.is_null());
}

#[test]
fn refused_jobs_match() {
    let ok = aac_case(48000, 4, 0);
    let mut cases = vec![
        Case {
            input_size: None,
            ..ok.clone()
        },
        Case {
            out_name: "SAME",
            ..ok.clone()
        },
    ];
    for edit in [
        |s: &mut OracleRemuxScript| s.open_ret = -2,
        |s: &mut OracleRemuxScript| s.find_info_ret = -5,
        |s: &mut OracleRemuxScript| s.alloc_ok = false,
        |s: &mut OracleRemuxScript| s.new_stream_fail_at = 0,
        |s: &mut OracleRemuxScript| {
            s.copy_fail_at = 0;
            s.copy_ret = -12;
        },
        |s: &mut OracleRemuxScript| s.avio_ret = -2,
    ] {
        let mut case = ok.clone();
        edit(&mut case.script);
        cases.push(case);
    }
    for case in &cases {
        compare(case);
    }
}

#[test]
fn stream_setup_matches() {
    for (codec_type, codec_id, channels) in [
        (0, H264, 0),
        (0, CODEC_ID_HEVC, 0),
        (MEDIA_TYPE_AUDIO, AAC, 1),
        (MEDIA_TYPE_AUDIO, AAC, 5),
        (MEDIA_TYPE_AUDIO, AAC, 6),
    ] {
        for (has_codec_tag, tag_codec_id) in
            [(true, CODEC_ID_HEVC), (true, 0), (false, CODEC_ID_HEVC)]
        {
            for flags in [0, FMT_NOFILE] {
                let mut case = aac_case(48000, 2, 0);
                case.script.streams[0] = stream(codec_type, codec_id, 48000, 1024, channels);
                case.script.has_codec_tag = has_codec_tag;
                case.script.tag_codec_id = tag_codec_id;
                case.script.out_flags = flags;
                compare(&case);
            }
        }
    }
}

#[test]
fn audio_timestamps_match() {
    for rate in [44100, 48000, 22050, 96000] {
        for gap in [0, 3] {
            compare(&aac_case(rate, 60, gap));
        }
    }
}

/// The cut between a short last frame and the input's rounding of a full one
/// (`samples - in_tick`): at 44.1 kHz in milliseconds a tick is 44.1
/// samples, rounded up to 45.
#[test]
fn short_last_frames_match() {
    for frame_duration in 940..1100 {
        for last in [21, 22, 23] {
            let mut case = aac_case(44100, 3, 0);
            case.script.streams[0].frame_duration = frame_duration;
            case.script.packets[2].duration = last;
            compare(&case);
        }
    }
}

#[test]
fn errors_and_progress_match() {
    let mut cases = Vec::new();
    for (header_ret, trailer_ret, read_end_ret) in
        [(-22, 0, ERROR_EOF), (0, -32, ERROR_EOF), (0, 0, -5)]
    {
        let mut case = aac_case(48000, 30, 0);
        case.script.header_ret = header_ret;
        case.script.trailer_ret = trailer_ret;
        case.script.read_end_ret = read_end_ret;
        cases.push(case);
    }
    for write_ret in [ERROR_INVALIDDATA, ERROR_EINVAL, -28] {
        let mut case = aac_case(48000, 30, 0);
        case.script.packets[5].write_ret = write_ret;
        cases.push(case);
    }
    for stop_at in [1, 2, 3] {
        cases.push(Case {
            stop_at,
            ..aac_case(48000, 40, 0)
        });
    }
    cases.push(Case {
        callback: false,
        ..aac_case(48000, 30, 0)
    });
    cases.push(Case {
        input_size: Some(0),
        ..aac_case(48000, 30, 0)
    });
    for case in &cases {
        compare(case);
    }
}

// ---------------------------------------------------------------------------
// random scripts

fn arb_rational() -> impl Strategy<Value = Rational> {
    prop_oneof![
        Just(Rational { num: 1, den: 1000 }),
        Just(Rational { num: 1, den: 90000 }),
        Just(Rational { num: 1, den: 48000 }),
        Just(Rational { num: 1, den: 44100 }),
        Just(Rational {
            num: 1001,
            den: 30000
        }),
        (1..4i32, 1..200_000i32).prop_map(|(num, den)| Rational { num, den }),
    ]
}

fn arb_stream() -> impl Strategy<Value = OracleRemuxStream> {
    (
        prop_oneof![Just(0), Just(MEDIA_TYPE_AUDIO), Just(2)],
        prop_oneof![Just(H264), Just(CODEC_ID_HEVC), Just(AAC)],
        prop_oneof![Just(0), Just(-1), Just(8000), Just(44100), Just(48000)],
        prop_oneof![Just(0), Just(-1), Just(960), Just(1024)],
        0..9i32,
        arb_rational(),
        arb_rational(),
        prop_oneof![Just(0), Just(-3), Just(960), Just(1024), Just(1152)],
    )
        .prop_map(
            |(
                codec_type,
                codec_id,
                sample_rate,
                frame_size,
                channels,
                in_time_base,
                out_time_base,
                frame_duration,
            )| {
                OracleRemuxStream {
                    codec_type,
                    codec_id,
                    sample_rate,
                    frame_size,
                    channels,
                    in_time_base,
                    out_time_base,
                    frame_duration,
                }
            },
        )
}

fn arb_ts() -> impl Strategy<Value = i64> {
    prop_oneof![
        4 => -100..100_000i64,
        1 => Just(NOPTS_VALUE),
        1 => Just(i64::MAX),
        1 => any::<i64>(),
    ]
}

fn arb_case() -> impl Strategy<Value = Case> {
    (
        prop::collection::vec(arb_stream(), 0..4),
        prop::collection::vec(
            (
                0..4i32,
                0..5000i32,
                arb_ts(),
                arb_ts(),
                prop_oneof![Just(0i64), -5..100i64, any::<i64>()],
                -1..1_000_000i64,
                prop_oneof![8 => Just(0), 1 => Just(ERROR_INVALIDDATA), 1 => Just(ERROR_EINVAL), 1 => Just(-28)],
            ),
            0..40,
        ),
        (any::<bool>(), any::<bool>(), any::<bool>(), 0..3u32),
        prop_oneof![Just(ERROR_EOF), Just(-5)],
        (0usize..4, any::<bool>(), 0usize..200_000),
    )
        .prop_map(|(streams, packets, (has_codec_tag, nofile, hevc, fail), read_end_ret, (stop_at, callback, size))| {
            let mut script = base_script();
            script.nb_streams = streams.len() as u32;
            for (i, s) in streams.iter().enumerate() {
                script.streams[i] = *s;
            }
            script.has_codec_tag = has_codec_tag;
            script.tag_codec_id = if hevc { CODEC_ID_HEVC } else { H264 };
            script.out_flags = if nofile { FMT_NOFILE } else { 0 };
            match fail {
                1 => script.header_ret = -22,
                2 => script.trailer_ret = -32,
                _ => {}
            }
            script.read_end_ret = read_end_ret;
            let mut n = 0;
            for (stream_index, size, pts, dts, duration, pos, write_ret) in packets {
                // Intentional difference: a packet of a stream that does
                // not exist (C: out of bounds).
                if stream_index as u32 >= script.nb_streams {
                    continue;
                }
                script.packets[n] = OracleRemuxPacket {
                    stream_index,
                    size,
                    pts,
                    dts,
                    duration,
                    pos,
                    write_ret,
                };
                n += 1;
            }
            script.nb_packets = n as u32;
            Case {
                script,
                input_size: Some(size),
                out_name: "out.mp4",
                callback,
                stop_at,
            }
        })
}

#[test]
fn random_scripts_match() {
    let mut runner = TestRunner::new(Config {
        cases: 256,
        failure_persistence: None,
        ..Config::default()
    });
    runner
        .run(&arb_case(), |case| {
            compare(&case);
            Ok(())
        })
        .unwrap();
}
