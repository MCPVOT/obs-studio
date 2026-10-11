//! Port of `libobs/media-io/format-conversion.c`: packing conversions
//! between UYVX, planar and semi-planar YUV, and packed 32-bit pixels.
//!
//! The C does the UYVX functions with SSE2 integer operations. Every lane
//! works out to plain integer math (picking bytes out of each pixel, and
//! `(a + b + c + d) >> 2` for the chroma of a 2x2 block), so this is a
//! scalar port, exact byte for byte. Offsets use the C's `u32` arithmetic,
//! wrapping included, and each loop keeps the C's order of reads and writes,
//! which decides the result where output rows overlap.
//!
//! The planes are [`Source`]s and [`Sink`]s addressed by byte offset. Byte
//! slices implement both and panic on an out-of-bounds access, where the C
//! reads or writes out of bounds; the C ABI shims implement them over raw
//! pointers. As in the C, a `uint16_t` or `uint32_t` value is stored in
//! native byte order.

/// An input plane, read by byte offset.
pub trait Source {
    /// The `N` bytes at `offset`.
    fn read<const N: usize>(&self, offset: usize) -> [u8; N];
}

/// An output plane, written by byte offset.
pub trait Sink {
    /// Writes `bytes` at `offset`.
    fn write<const N: usize>(&mut self, offset: usize, bytes: [u8; N]);
}

impl Source for [u8] {
    fn read<const N: usize>(&self, offset: usize) -> [u8; N] {
        let mut bytes = [0; N];
        bytes.copy_from_slice(&self[offset..offset + N]);
        bytes
    }
}

impl Sink for [u8] {
    fn write<const N: usize>(&mut self, offset: usize, bytes: [u8; N]) {
        self[offset..offset + N].copy_from_slice(&bytes);
    }
}

/// Packs UYVX rows `start_y..end_y`, two at a time, into planar I420:
/// full-size luma in `output[0]`, and U and V averaged over each 2x2 block
/// in `output[1]` and `output[2]`.
///
/// Each UYVX pixel is 4 bytes, U Y V X. The width is
/// `min(in_linesize, out_linesize[0])` pixels, converted 4 at a time. Luma
/// rows are `out_linesize[0]` apart and both chroma planes'
/// `out_linesize[1]` apart: `out_linesize[2]` is not used.
pub fn compress_uyvx_to_i420<S, D>(
    input: &S,
    in_linesize: u32,
    start_y: u32,
    end_y: u32,
    output: [&mut D; 3],
    out_linesize: &[u32],
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (input, in_linesize, start_y, end_y, output, out_linesize);
    todo!()
}

/// Packs UYVX rows `start_y..end_y`, two at a time, into NV12: full-size
/// luma in `output[0]`, and U and V averaged over each 2x2 block and
/// interleaved in `output[1]`.
///
/// The width is `min(in_linesize, out_linesize[0])` pixels, converted 4 at
/// a time. Luma rows are `out_linesize[0]` apart, chroma rows
/// `out_linesize[1]`.
pub fn compress_uyvx_to_nv12<S, D>(
    input: &S,
    in_linesize: u32,
    start_y: u32,
    end_y: u32,
    output: [&mut D; 2],
    out_linesize: &[u32],
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (input, in_linesize, start_y, end_y, output, out_linesize);
    todo!()
}

/// Splits UYVX rows `start_y..end_y`, two at a time, into full-size Y, U
/// and V planes in `output[0..3]`.
///
/// The width is `min(in_linesize, out_linesize)` pixels, converted 4 at a
/// time. All three planes' rows are `out_linesize` apart: the C reads only
/// `out_linesize[0]`.
pub fn convert_uyvx_to_i444<S, D>(
    input: &S,
    in_linesize: u32,
    start_y: u32,
    end_y: u32,
    output: [&mut D; 3],
    out_linesize: u32,
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (input, in_linesize, start_y, end_y, output, out_linesize);
    todo!()
}

/// Expands planar I420 into one `Y << 16 | U << 8 | V` value per pixel.
///
/// Rows go in pairs from `start_y / 2` to `end_y / 2`, both rounded down.
/// The width is `in_linesize[0]` rounded down to even, whatever
/// `out_linesize` is.
pub fn decompress_420<S, D>(
    input: [&S; 3],
    in_linesize: &[u32],
    start_y: u32,
    end_y: u32,
    output: &mut D,
    out_linesize: u32,
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (input, in_linesize, start_y, end_y, output, out_linesize);
    todo!()
}

/// Expands NV12 into one `Y | uv << 8` value per pixel, where `uv` is the
/// interleaved U V pair read as a native-endian `u16`.
///
/// Rows go in pairs from `start_y / 2` to `end_y / 2`, both rounded down.
/// The width is `min(in_linesize[0], out_linesize)` rounded down to even.
pub fn decompress_nv12<S, D>(
    input: [&S; 2],
    in_linesize: &[u32],
    start_y: u32,
    end_y: u32,
    output: &mut D,
    out_linesize: u32,
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (input, in_linesize, start_y, end_y, output, out_linesize);
    todo!()
}

/// Expands packed 4:2:2 rows `start_y..end_y` into one 32-bit value per
/// pixel: each input `u32` is copied, then repeated with its other luma byte
/// in place of the first (`leading_lum`, YUYV) or second (UYVY) one.
///
/// Characterized, not endorsed: each row reads
/// `min(in_linesize, out_linesize) / 2` values, twice the row, and writes
/// twice as many, so each row spills into the next one, which then
/// overwrites the spill.
pub fn decompress_422<S, D>(
    input: &S,
    in_linesize: u32,
    start_y: u32,
    end_y: u32,
    output: &mut D,
    out_linesize: u32,
    leading_lum: bool,
) where
    S: Source + ?Sized,
    D: Sink + ?Sized,
{
    let _ = (
        input,
        in_linesize,
        start_y,
        end_y,
        output,
        out_linesize,
        leading_lum,
    );
    todo!()
}
