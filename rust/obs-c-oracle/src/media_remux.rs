//! `oracle_*` build of `libobs/media-io/media-remux.c.h`
//! (oracle/media_remux.c), the media-remux adapter the Rust port calls
//! (oracle/media_remux_adapter.c), and the recording FFmpeg stand-in both
//! run over (oracle/media_remux_ffmpeg.c, oracle/ffmpeg-shim/). A test loads
//! a script with [`oracle_remux_ffmpeg_reset`], runs one side, and reads the
//! log of every FFmpeg call and `blog` line with [`oracle_remux_ffmpeg_log`].

use core::ffi::{c_char, c_void};

/// Streams and packets a script holds at most.
pub const MAX_STREAMS: usize = 8;
pub const MAX_PACKETS: usize = 64;

/// `AVRational`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct OracleRational {
    pub num: i32,
    pub den: i32,
}

/// `struct oracle_remux_stream`: one input stream.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct OracleRemuxStream {
    pub codec_type: i32,
    pub codec_id: i32,
    pub sample_rate: i32,
    pub frame_size: i32,
    pub channels: i32,
    pub in_time_base: OracleRational,
    /// What the muxer's `avformat_write_header` sets.
    pub out_time_base: OracleRational,
    /// What `av_get_audio_frame_duration2` returns for the stream.
    pub frame_duration: i32,
}

/// `struct oracle_remux_packet`: one packet `av_read_frame` returns.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq, Eq)]
pub struct OracleRemuxPacket {
    pub stream_index: i32,
    pub size: i32,
    pub pts: i64,
    pub dts: i64,
    pub duration: i64,
    pub pos: i64,
    /// What `av_interleaved_write_frame` returns for it.
    pub write_ret: i32,
}

/// `struct oracle_remux_script`: the input file and every call's result.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct OracleRemuxScript {
    pub open_ret: i32,
    pub find_info_ret: i32,
    pub alloc_ok: bool,
    pub out_flags: i32,
    pub has_codec_tag: bool,
    /// What `av_codec_get_id` returns for any tag.
    pub tag_codec_id: i32,
    pub nb_streams: u32,
    /// `avformat_new_stream` returns NULL for this output stream.
    pub new_stream_fail_at: u32,
    /// `avcodec_parameters_copy` returns `copy_ret` for this stream.
    pub copy_fail_at: u32,
    pub copy_ret: i32,
    pub avio_ret: i32,
    pub header_ret: i32,
    pub trailer_ret: i32,
    /// What `av_read_frame` returns after the last packet.
    pub read_end_ret: i32,
    pub nb_packets: u32,
    pub streams: [OracleRemuxStream; MAX_STREAMS],
    pub packets: [OracleRemuxPacket; MAX_PACKETS],
}

impl Default for OracleRemuxScript {
    fn default() -> Self {
        Self {
            open_ret: 0,
            find_info_ret: 0,
            alloc_ok: false,
            out_flags: 0,
            has_codec_tag: false,
            tag_codec_id: 0,
            nb_streams: 0,
            new_stream_fail_at: 0,
            copy_fail_at: 0,
            copy_ret: 0,
            avio_ret: 0,
            header_ret: 0,
            trailer_ret: 0,
            read_end_ret: 0,
            nb_packets: 0,
            streams: [OracleRemuxStream::default(); MAX_STREAMS],
            packets: [OracleRemuxPacket::default(); MAX_PACKETS],
        }
    }
}

/// `media_remux_progress_callback`.
pub type OracleRemuxProgress = unsafe extern "C" fn(data: *mut c_void, percent: f32) -> bool;

unsafe extern "C" {
    /// `job` points to a `media_remux_job_t`.
    pub fn oracle_media_remux_job_create(
        job: *mut *mut c_void,
        in_filename: *const c_char,
        out_filename: *const c_char,
    ) -> bool;
    pub fn oracle_media_remux_job_process(
        job: *mut c_void,
        callback: Option<OracleRemuxProgress>,
        data: *mut c_void,
    ) -> bool;
    pub fn oracle_media_remux_job_destroy(job: *mut c_void);

    /// Loads `script` and clears the log.
    pub fn oracle_remux_ffmpeg_reset(script: *const OracleRemuxScript);
    /// Copies up to `size` bytes of the log to `out`; returns its length.
    pub fn oracle_remux_ffmpeg_log(out: *mut c_char, size: usize) -> usize;
    /// FFmpeg objects handed out and not freed.
    pub fn oracle_remux_ffmpeg_live() -> usize;

    /// Writes the size, the alignment and every field offset of the
    /// adapter's `struct obs_media_remux_stream`, then the same for
    /// `struct obs_media_remux_packet`, to `out` (at least 16 values) and
    /// returns how many it wrote.
    pub fn oracle_media_remux_adapter_layout(out: *mut usize) -> usize;
}
