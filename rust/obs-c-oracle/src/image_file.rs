//! `oracle_*` build of `libobs/graphics/image-file.c.h`
//! (oracle/image_file.c). It and the Rust shim call the same libnsgif
//! (built by build.rs) and the same recording `gs_*` stand-in
//! (oracle/texture_render_device.c, reset and read through
//! [`crate::texture_render`]), which also stands in for
//! `gs_create_texture_file_data3` and `gs_texture_set_image`.

use core::ffi::{c_char, c_void};

unsafe extern "C" {
    /// `image` points to a `struct gs_image_file_ex`.
    pub fn oracle_gs_image_file_ex_init(image: *mut c_void, file: *const c_char, alpha_mode: u32);
    pub fn oracle_gs_image_file_ex_free(image: *mut c_void);
    pub fn oracle_gs_image_file_ex_init_texture(image: *mut c_void);
    pub fn oracle_gs_image_file_ex_tick(image: *mut c_void, elapsed_time_ns: u64) -> bool;
    pub fn oracle_gs_image_file_ex_update_texture(image: *mut c_void);

    /// Writes the size, the alignment and every field offset of
    /// `struct gs_image_file_ex`, in declaration order, to `out` (at least
    /// 17 values) and returns how many it wrote.
    pub fn oracle_image_file_layout(out: *mut usize) -> usize;
}
