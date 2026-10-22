//! C ABI for `libobs/obs-scene-group-transform.c.h`.
//!
//! Each shim reads its inputs, then writes the outputs. `obs-scene.c`
//! passes a distinct position, scale, and rotation. A null pointer is not
//! checked. The C file has no `EXPORT`, so the symbols stay hidden.

use std::ptr;

use obs_graphics::ffi::matrix4::matrix4;
use obs_graphics::ffi::vec2::vec2;
use obs_graphics::matrix4::Matrix4;
use obs_graphics::vec2::Vec2;

use crate::group_transform::{group_transform, transform_val};

/// Same contract as C `obs_scene_transform_val`.
///
/// # Safety
///
/// `dst` must be readable and writable. `transform` must be readable.
/// They do not alias in `obs-scene.c`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_transform_val(dst: *mut vec2, transform: *const matrix4) {
    // SAFETY: the caller guarantees both pointers are readable.
    let (point, m) = unsafe { (ptr::read(dst), ptr::read(transform)) };
    let out = transform_val(Vec2::from(point), &Matrix4::from(m));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Same contract as C `obs_scene_group_transform`.
///
/// # Safety
///
/// `transform` must be readable. `pos`, `scale`, and `rot` must be
/// readable and writable. `obs-scene.c` passes three distinct objects,
/// and it does not alias them with `transform`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_group_transform(
    transform: *const matrix4,
    pos: *mut vec2,
    scale: *mut vec2,
    rot: *mut f32,
    rot_delta: f32,
) {
    // SAFETY: the caller guarantees every pointer is readable.
    let (m, pos_in, scale_in, rot_in) = unsafe {
        (
            ptr::read(transform),
            ptr::read(pos),
            ptr::read(scale),
            ptr::read(rot),
        )
    };
    let out = group_transform(
        &Matrix4::from(m),
        Vec2::from(pos_in),
        Vec2::from(scale_in),
        rot_in,
        rot_delta,
    );
    // SAFETY: the caller guarantees the three outputs are writable.
    unsafe {
        ptr::write(pos, out.pos.into());
        ptr::write(scale, out.scale.into());
        ptr::write(rot, out.rot);
    }
}
