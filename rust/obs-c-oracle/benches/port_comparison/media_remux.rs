//! `libobs/media-io/media-remux.c.h`: one whole remux job (create, process,
//! destroy) of a 48 kHz AAC track of 60 frames stored in milliseconds, as an
//! MKV recording is, with a progress callback. Both sides run over the
//! oracle's recording FFmpeg stand-in, the Rust port through its C adapter,
//! so the numbers compare the job logic plus the same FFmpeg stand-in work.

use std::ffi::{CString, c_void};
use std::hint::black_box;
use std::ptr;

use criterion::Criterion;
use obs_c_oracle::media_remux::{
    MAX_PACKETS, OracleRational, OracleRemuxPacket, OracleRemuxScript, OracleRemuxStream,
    oracle_media_remux_job_create, oracle_media_remux_job_destroy, oracle_media_remux_job_process,
    oracle_remux_ffmpeg_reset,
};
use obs_media_io::ffi::media_remux::{
    media_remux_job_create, media_remux_job_destroy, media_remux_job_process,
};
use obs_media_io::media_remux::ERROR_EOF;

const FRAMES: usize = 60;
const _: () = assert!(FRAMES <= MAX_PACKETS);
const AUDIO: i32 = 1;
const AAC: i32 = 86018;

unsafe extern "C" fn progress(data: *mut c_void, percent: f32) -> bool {
    // SAFETY: a `f32` for the duration of the call.
    unsafe { *data.cast::<f32>() = percent };
    true
}

fn script() -> OracleRemuxScript {
    let mut script = OracleRemuxScript {
        alloc_ok: true,
        nb_streams: 1,
        new_stream_fail_at: u32::MAX,
        copy_fail_at: u32::MAX,
        read_end_ret: ERROR_EOF,
        nb_packets: FRAMES as u32,
        ..OracleRemuxScript::default()
    };
    script.streams[0] = OracleRemuxStream {
        codec_type: AUDIO,
        codec_id: AAC,
        sample_rate: 48000,
        frame_size: 1024,
        channels: 2,
        in_time_base: OracleRational { num: 1, den: 1000 },
        out_time_base: OracleRational { num: 1, den: 48000 },
        frame_duration: 1024,
    };
    for i in 0..FRAMES as i64 {
        let start = (i * 1024 * 1000 + 24_000) / 48_000;
        let end = ((i + 1) * 1024 * 1000 + 24_000) / 48_000;
        script.packets[i as usize] = OracleRemuxPacket {
            stream_index: 0,
            size: 300,
            pts: start,
            dts: start,
            duration: end - start,
            pos: 4096 + i * 300,
            write_ret: 0,
        };
    }
    script
}

pub fn benchmarks(criterion: &mut Criterion) {
    let script = script();
    let dir = std::env::temp_dir().join(format!("obs-media-remux-bench-{}", std::process::id()));
    std::fs::create_dir_all(&dir).expect("bench directory");
    let input = dir.join("in.mkv");
    std::fs::write(&input, vec![0u8; 4096 + FRAMES * 300]).expect("bench input");
    let input = CString::new(input.to_str().expect("UTF-8 path")).expect("no NUL");
    let output = CString::new(dir.join("out.mp4").to_str().expect("UTF-8 path")).expect("no NUL");

    let rust = || {
        let mut last = 0.0f32;
        let mut job = ptr::null_mut();
        // SAFETY: a plain script copy, valid strings, and `last` outlives
        // the job.
        unsafe {
            oracle_remux_ffmpeg_reset(&script);
            assert!(media_remux_job_create(
                &mut job,
                input.as_ptr(),
                output.as_ptr()
            ));
            let ok = media_remux_job_process(job, Some(progress), (&raw mut last).cast());
            media_remux_job_destroy(job);
            (ok, black_box(last).to_bits())
        }
    };
    let original = || {
        let mut last = 0.0f32;
        let mut job = ptr::null_mut();
        // SAFETY: as above.
        unsafe {
            oracle_remux_ffmpeg_reset(&script);
            assert!(oracle_media_remux_job_create(
                &mut job,
                input.as_ptr(),
                output.as_ptr()
            ));
            let ok = oracle_media_remux_job_process(job, Some(progress), (&raw mut last).cast());
            oracle_media_remux_job_destroy(job);
            (ok, black_box(last).to_bits())
        }
    };
    super::pair(criterion, "media_remux/job_60_aac_frames", rust, original);
    let _ = std::fs::remove_dir_all(&dir);
}
