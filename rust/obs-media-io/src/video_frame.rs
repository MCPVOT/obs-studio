//! Port of `libobs/media-io/video-frame.c` and the `static inline` helpers
//! in `video-frame.h`.
//!
//! A frame is one allocation holding every plane, each padded to the bmem
//! alignment. [`FrameLayout`] is where `video_frame_init` puts the planes,
//! and [`VideoFrame`] owns a buffer laid out that way. The arithmetic is the
//! C's: `u32` linesizes wrap, and the plane sizes and offsets are `usize`.

use crate::video_io::{MAX_AV_PLANES, VideoFormat};

/// `video_frame_get_linesizes`: the bytes per row of each plane of a
/// `width`-pixel frame, 0 for the planes `format` does not have.
///
/// Characterized, not endorsed: V210 rounds `width` up with the C's `ALIGN`
/// to 48, but `ALIGN` masks with `!(alignment - 1)`, which only rounds to a
/// power of two. Width 1 becomes 16 and 1280 stays 1280, so the linesize is
/// 3424, not 3456. It is never below `width` unless `width + 47` wraps, so a
/// row still fits.
pub fn linesizes(format: VideoFormat, width: u32) -> [u32; MAX_AV_PLANES] {
    let _ = (format, width);
    todo!("port video_frame_get_linesizes")
}

/// `video_frame_get_plane_heights`: the rows of each plane of a
/// `height`-row frame, 0 for the planes `format` does not have.
pub fn plane_heights(format: VideoFormat, height: u32) -> [u32; MAX_AV_PLANES] {
    let _ = (format, height);
    todo!("port video_frame_get_plane_heights")
}

/// Where `video_frame_init` puts each plane of a frame.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct FrameLayout {
    /// Bytes to allocate for all planes. 0 when no plane has both a
    /// linesize and a height, which `bmalloc` refuses.
    pub size: usize,
    /// The frame's `linesize`: plane 0's always, then each plane that has
    /// both a linesize and a height.
    pub linesize: [u32; MAX_AV_PLANES],
    /// Each plane's byte offset from `data[0]`, `None` where `data[i]` stays
    /// null. Plane 0 is always at 0.
    ///
    /// Characterized, not endorsed: plane `i` starts where plane `i - 1`
    /// ended, or at 0 if plane `i - 1` was skipped. A skip only happens when
    /// a linesize wraps to 0 (e.g. I40A at width `u32::MAX`, where planes 1
    /// and 2 are skipped and plane 3 lands on plane 0).
    pub offset: [Option<usize>; MAX_AV_PLANES],
}

impl FrameLayout {
    /// The layout `video_frame_init` gives a `width` by `height` frame.
    pub fn new(format: VideoFormat, width: u32, height: u32) -> Self {
        let _ = (format, width, height);
        todo!("port the video_frame_init layout")
    }
}

/// The bytes of `(dst, src)` that copying `rows` rows of one plane touches,
/// counted from the plane's start: `linesize * rows`, wrapped to `u32`, when
/// the linesizes match, else up to the end of the last row's shorter
/// linesize. `(0, 0)` when nothing is copied.
pub fn copy_extent(dst_linesize: u32, src_linesize: u32, rows: u32) -> (usize, usize) {
    let _ = (dst_linesize, src_linesize, rows);
    todo!("port the video_frame_copy extents")
}

/// One plane of `video_frame_copy`: copies `rows` rows from `src` to `dst`.
/// Equal linesizes copy `linesize * rows` bytes (wrapped to `u32`) in one
/// go; otherwise each row copies the shorter linesize, leaving the rest of a
/// wider `dst` row untouched.
///
/// # Panics
///
/// If `dst` or `src` is shorter than [`copy_extent`].
pub fn copy_plane(dst: &mut [u8], dst_linesize: u32, src: &[u8], src_linesize: u32, rows: u32) {
    let _ = (dst, dst_linesize, src, src_linesize, rows);
    todo!("port the video_frame_copy plane loop")
}

/// An owned frame laid out as `video_frame_init` lays it out
/// (`video_frame_create`; dropping it is `video_frame_destroy`).
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct VideoFrame {
    data: Box<[u8]>,
    layout: FrameLayout,
}

impl VideoFrame {
    /// A zeroed `width` by `height` frame.
    ///
    /// # Panics
    ///
    /// If the layout's size is 0 (`VideoFormat::None`, or a zero width or
    /// height), where C's `bmalloc(0)` crashes.
    pub fn new(format: VideoFormat, width: u32, height: u32) -> Self {
        let _ = (format, width, height);
        todo!("port video_frame_create")
    }

    pub fn layout(&self) -> &FrameLayout {
        &self.layout
    }

    pub fn linesize(&self, plane: usize) -> u32 {
        self.layout.linesize[plane]
    }

    /// The frame's bytes from `plane`'s start to the end of the buffer, or
    /// `None` for a plane the frame has no pointer for.
    pub fn plane(&self, plane: usize) -> Option<&[u8]> {
        Some(&self.data[self.layout.offset[plane]?..])
    }

    /// As [`Self::plane`], mutably.
    pub fn plane_mut(&mut self, plane: usize) -> Option<&mut [u8]> {
        Some(&mut self.data[self.layout.offset[plane]?..])
    }

    /// `video_frame_copy`: copies the first `cy` rows of every plane of
    /// `format` from `src`, row by row where the linesizes differ.
    ///
    /// # Panics
    ///
    /// If either frame is too small for `cy` rows of `format`, or lacks a
    /// plane with bytes to copy.
    pub fn copy_from(&mut self, src: &VideoFrame, format: VideoFormat, cy: u32) {
        let _ = (src, format, cy);
        todo!("port video_frame_copy")
    }
}
