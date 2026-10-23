//! Tier 2: the `#[repr(C)]` structs the Rust remux passes to the private C
//! adapter (`media-io/media-remux-ffmpeg.c.h`) against the C layouts.

use core::mem::{align_of, offset_of, size_of};

use obs_c_oracle::media_remux::oracle_media_remux_adapter_layout;
use obs_media_io::media_remux::{Packet, StreamInfo};

#[test]
fn adapter_structs_match_c() {
    let rust = [
        size_of::<StreamInfo>(),
        align_of::<StreamInfo>(),
        offset_of!(StreamInfo, codec_type),
        offset_of!(StreamInfo, codec_id),
        offset_of!(StreamInfo, sample_rate),
        offset_of!(StreamInfo, frame_size),
        offset_of!(StreamInfo, channels),
        offset_of!(StreamInfo, time_base),
        size_of::<Packet>(),
        align_of::<Packet>(),
        offset_of!(Packet, pts),
        offset_of!(Packet, dts),
        offset_of!(Packet, duration),
        offset_of!(Packet, pos),
        offset_of!(Packet, size),
        offset_of!(Packet, stream_index),
    ];
    let mut c = [0usize; 16];
    // SAFETY: room for every value it writes.
    let n = unsafe { oracle_media_remux_adapter_layout(c.as_mut_ptr()) };
    assert_eq!(n, c.len());
    assert_eq!(rust, c);
}
