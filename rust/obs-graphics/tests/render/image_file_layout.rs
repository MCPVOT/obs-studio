//! `gs_image_file_ex_t` matches `struct gs_image_file_ex` in
//! `libobs/graphics/image-file.h`.

use core::mem::{align_of, offset_of, size_of};

use obs_c_oracle::image_file as c;
use obs_graphics::ffi::image_file::gs_image_file_ex_t;

#[test]
fn image_file_layout_matches_c() {
    let mut out = [0usize; 32];
    // SAFETY: `out` has room for every value the oracle writes.
    let n = unsafe { c::oracle_image_file_layout(out.as_mut_ptr()) };
    let rust = [
        size_of::<gs_image_file_ex_t>(),
        align_of::<gs_image_file_ex_t>(),
        offset_of!(gs_image_file_ex_t, texture),
        offset_of!(gs_image_file_ex_t, format),
        offset_of!(gs_image_file_ex_t, cx),
        offset_of!(gs_image_file_ex_t, cy),
        offset_of!(gs_image_file_ex_t, is_animated_gif),
        offset_of!(gs_image_file_ex_t, frame_updated),
        offset_of!(gs_image_file_ex_t, loaded),
        offset_of!(gs_image_file_ex_t, internal),
        offset_of!(gs_image_file_ex_t, cur_time),
        offset_of!(gs_image_file_ex_t, cur_frame),
        offset_of!(gs_image_file_ex_t, cur_loop),
        offset_of!(gs_image_file_ex_t, texture_data),
        offset_of!(gs_image_file_ex_t, mem_usage),
        offset_of!(gs_image_file_ex_t, alpha_mode),
        offset_of!(gs_image_file_ex_t, space),
    ];
    assert_eq!(&out[..n], &rust[..]);
}
