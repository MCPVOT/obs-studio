//! C ABI for `libobs/obs-scene-coords.c.h`.
//!
//! Each shim reads its input vector, then writes `dst`, so the caller may
//! pass the same pointer for both. A null pointer is not checked.
//! `obs-scene.c` calls these inside libobs. The C file has no `EXPORT`,
//! so the symbols stay hidden.

use std::ptr;

use obs_graphics::ffi::vec2::vec2;
use obs_graphics::vec2::Vec2;

use crate::coords::{
    coords_keep_scale, coords_nudge, item_canvas_scale, item_relative_scale, nudge_abs,
    pos_from_absolute, pos_to_absolute, size_from_absolute, size_to_absolute,
};

/// # Safety
///
/// `v` must be readable.
unsafe fn read_vec(v: *const vec2) -> Vec2 {
    // SAFETY: the caller guarantees `v` is readable.
    Vec2::from(unsafe { ptr::read(v) })
}

/// # Safety
///
/// `dst` must be writable.
unsafe fn write_vec(dst: *mut vec2, value: Vec2) {
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, value.into()) }
}

/// Same contract as C `obs_scene_coords_nudge`. No pointers.
#[unsafe(no_mangle)]
pub extern "C" fn obs_scene_coords_nudge(is_group: bool, parent_is_group: bool) -> bool {
    coords_nudge(is_group, parent_is_group)
}

/// Same contract as C `obs_scene_coords_keep_scale`. No pointers.
#[unsafe(no_mangle)]
pub extern "C" fn obs_scene_coords_keep_scale(is_group: bool, is_scene: bool) -> bool {
    coords_keep_scale(is_group, is_scene)
}

/// Same contract as C `obs_scene_nudge_abs`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_nudge_abs(dst: *mut vec2, v: *const vec2) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, nudge_abs(value)) }
}

/// Same contract as C `obs_scene_pos_to_absolute`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_pos_to_absolute(
    dst: *mut vec2,
    v: *const vec2,
    canvas_x: f32,
    canvas_y: f32,
    nudge: bool,
) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, pos_to_absolute(value, canvas_x, canvas_y, nudge)) }
}

/// Same contract as C `obs_scene_pos_from_absolute`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_pos_from_absolute(
    dst: *mut vec2,
    v: *const vec2,
    canvas_x: f32,
    canvas_y: f32,
) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, pos_from_absolute(value, canvas_x, canvas_y)) }
}

/// Same contract as C `obs_scene_size_to_absolute`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_size_to_absolute(
    dst: *mut vec2,
    v: *const vec2,
    canvas_y: f32,
    nudge: bool,
) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, size_to_absolute(value, canvas_y, nudge)) }
}

/// Same contract as C `obs_scene_size_from_absolute`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_size_from_absolute(
    dst: *mut vec2,
    v: *const vec2,
    canvas_y: f32,
) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, size_from_absolute(value, canvas_y)) }
}

/// Same contract as C `obs_scene_item_canvas_scale`.
///
/// # Safety
///
/// `dst` must be writable and `scale` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_item_canvas_scale(
    dst: *mut vec2,
    scale: *const vec2,
    canvas_y: f32,
    scale_ref_y: f32,
) {
    let value = unsafe { read_vec(scale) };
    unsafe { write_vec(dst, item_canvas_scale(value, canvas_y, scale_ref_y)) }
}

/// Same contract as C `obs_scene_item_relative_scale`.
///
/// # Safety
///
/// `dst` must be writable and `v` readable. They may alias.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_scene_item_relative_scale(
    dst: *mut vec2,
    v: *const vec2,
    canvas_y: f32,
    scale_ref_y: f32,
) {
    let value = unsafe { read_vec(v) };
    unsafe { write_vec(dst, item_relative_scale(value, canvas_y, scale_ref_y)) }
}
