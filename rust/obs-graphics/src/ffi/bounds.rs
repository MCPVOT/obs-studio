//! C ABI shims for the exported functions in `libobs/graphics/bounds.h`.
//!
//! The shims only convert to the safe core in [`crate::bounds`]. Each reads
//! its inputs before writing `dst`, so a caller may alias `dst` with an input.
//! `bounds_get_point` writes only x, y, and z, and writes nothing when
//! `i > 8`. A ray or line miss does not write `*t`.
//!
//! Header-inline helpers (`bounds_zero`, `bounds_copy`, `bounds_inside`,
//! `bounds_vec3_inside`, `bounds_intersects_ray`, `bounds_intersects_line`)
//! stay in the C header.

use core::ptr;

use super::matrix3::matrix3;
use super::matrix4::matrix4;
use super::plane::plane;
use super::vec3::vec3;
use crate::bounds::Bounds;
use crate::matrix3::Matrix3;
use crate::matrix4::Matrix4;
use crate::plane::Plane;
use crate::vec3::Vec3;

/// `struct bounds`: two `vec3`s, size 32, align 16.
#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct bounds {
    pub min: vec3,
    pub max: vec3,
}

impl From<bounds> for Bounds {
    fn from(b: bounds) -> Self {
        Self::new(b.min.into(), b.max.into())
    }
}

impl From<Bounds> for bounds {
    fn from(b: Bounds) -> Self {
        Self {
            min: b.min.into(),
            max: b.max.into(),
        }
    }
}

/// Writes `*b` translated by `*v` to `*dst`.
///
/// # Safety
///
/// `b` and `v` must be readable and `dst` writable. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_move(dst: *mut bounds, b: *const bounds, v: *const vec3) {
    // SAFETY: the caller guarantees `b` and `v` are readable.
    let (src, delta) = unsafe { (ptr::read(b), ptr::read(v)) };
    let out = Bounds::from(src).move_by(Vec3::from(delta));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Writes `*b` scaled by `*v` to `*dst`. `w` is kept.
///
/// # Safety
///
/// `b` and `v` must be readable and `dst` writable. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_scale(dst: *mut bounds, b: *const bounds, v: *const vec3) {
    // SAFETY: the caller guarantees `b` and `v` are readable.
    let (src, factor) = unsafe { (ptr::read(b), ptr::read(v)) };
    let out = Bounds::from(src).scale(Vec3::from(factor));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Writes the merged box of `*b1` and `*b2` to `*dst`.
///
/// # Safety
///
/// The three pointers follow the usual read-then-write alias rule.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_merge(dst: *mut bounds, b1: *const bounds, b2: *const bounds) {
    // SAFETY: the caller guarantees `b1` and `b2` are readable.
    let (a, b) = unsafe { (ptr::read(b1), ptr::read(b2)) };
    let out = Bounds::from(a).merge(Bounds::from(b));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Writes `*b` grown to include `*v` to `*dst`.
///
/// # Safety
///
/// `b` and `v` must be readable and `dst` writable. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_merge_point(dst: *mut bounds, b: *const bounds, v: *const vec3) {
    // SAFETY: the caller guarantees `b` and `v` are readable.
    let (src, p) = unsafe { (ptr::read(b), ptr::read(v)) };
    let out = Bounds::from(src).merge_point(Vec3::from(p));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Writes corner `i` of `*b` into x, y, and z of `*dst`. `w` is left as it was.
/// `i > 8` writes nothing.
///
/// # Safety
///
/// `b` must be readable. `dst` must be writable when `i <= 8`. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_get_point(dst: *mut vec3, b: *const bounds, i: u32) {
    // SAFETY: the caller guarantees `b` is readable.
    let src = unsafe { ptr::read(b) };
    if let Some(p) = Bounds::from(src).point(i) {
        // SAFETY: the caller guarantees `dst` is writable. Only xyz change.
        unsafe {
            (*dst).x = p.x;
            (*dst).y = p.y;
            (*dst).z = p.z;
        }
    }
}

/// Writes the center of `*b` to `*dst`.
///
/// # Safety
///
/// `b` must be readable and `dst` writable. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_get_center(dst: *mut vec3, b: *const bounds) {
    // SAFETY: the caller guarantees `b` is readable.
    let src = unsafe { ptr::read(b) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Bounds::from(src).center().into()) };
}

/// Writes `*b` transformed by `*m` to `*dst`.
///
/// # Safety
///
/// `b` and `m` must be readable and `dst` writable. `dst` may alias `b`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_transform(dst: *mut bounds, b: *const bounds, m: *const matrix4) {
    // SAFETY: the caller guarantees `b` and `m` are readable.
    let (src, mat) = unsafe { (ptr::read(b), ptr::read(m)) };
    let out = Bounds::from(src).transform(&Matrix4::from(mat));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Writes `*b` transformed by `*m` to `*dst`.
///
/// # Safety
///
/// `b` and `m` must be readable and `dst` writable. `dst` may alias `b`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_transform3x4(
    dst: *mut bounds,
    b: *const bounds,
    m: *const matrix3,
) {
    // SAFETY: the caller guarantees `b` and `m` are readable.
    let (src, mat) = unsafe { (ptr::read(b), ptr::read(m)) };
    let out = Bounds::from(src).transform3x4(&Matrix3::from(mat));
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, out.into()) };
}

/// Ray test. A miss leaves `*t` unchanged.
///
/// # Safety
///
/// `b`, `orig`, and `dir` must be readable and `t` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_intersection_ray(
    b: *const bounds,
    orig: *const vec3,
    dir: *const vec3,
    t: *mut f32,
) -> bool {
    // SAFETY: the caller guarantees the three inputs are readable.
    let (src, o, d) = unsafe { (ptr::read(b), ptr::read(orig), ptr::read(dir)) };
    if let Some(hit) = Bounds::from(src).intersection_ray(Vec3::from(o), Vec3::from(d)) {
        // SAFETY: the caller guarantees `t` is writable.
        unsafe { ptr::write(t, hit) };
        true
    } else {
        false
    }
}

/// Segment test. A miss leaves `*t` unchanged.
///
/// # Safety
///
/// `b`, `p1`, and `p2` must be readable and `t` writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_intersection_line(
    b: *const bounds,
    p1: *const vec3,
    p2: *const vec3,
    t: *mut f32,
) -> bool {
    // SAFETY: the caller guarantees the three inputs are readable.
    let (src, a, c) = unsafe { (ptr::read(b), ptr::read(p1), ptr::read(p2)) };
    if let Some(hit) = Bounds::from(src).intersection_line(Vec3::from(a), Vec3::from(c)) {
        // SAFETY: the caller guarantees `t` is writable.
        unsafe { ptr::write(t, hit) };
        true
    } else {
        false
    }
}

/// Plane classification. The body returns 1, 2, or 3, and the C `bool`
/// conversion stores 1 for every branch. The safe core keeps the code.
///
/// # Safety
///
/// `b` and `p` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_plane_test(b: *const bounds, p: *const plane) -> bool {
    // SAFETY: the caller guarantees both pointers are readable.
    let (src, pl) = unsafe { (ptr::read(b), ptr::read(p)) };
    // Every C return is a nonzero code, so the ABI byte is 1.
    let _code = Bounds::from(src).plane_test(&Plane::from(pl));
    true
}

/// Whether every point of `*b` is under `*p`, with the corner `w` set to 0.
///
/// # Safety
///
/// `b` and `p` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_under_plane(b: *const bounds, p: *const plane) -> bool {
    // SAFETY: the caller guarantees both pointers are readable.
    let (src, pl) = unsafe { (ptr::read(b), ptr::read(p)) };
    Bounds::from(src).under_plane(&Plane::from(pl))
}

/// AABB overlap at `epsilon`.
///
/// # Safety
///
/// `b` and `test` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_intersects(
    b: *const bounds,
    test: *const bounds,
    epsilon: f32,
) -> bool {
    // SAFETY: the caller guarantees both pointers are readable.
    let (src, other) = unsafe { (ptr::read(b), ptr::read(test)) };
    Bounds::from(src).intersects(&Bounds::from(other), epsilon)
}

/// OBB overlap. A singular `*m` uses a zero matrix.
///
/// # Safety
///
/// `b`, `test`, and `m` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_intersects_obb(
    b: *const bounds,
    test: *const bounds,
    m: *const matrix4,
    epsilon: f32,
) -> bool {
    // SAFETY: the caller guarantees the three inputs are readable.
    let (src, other, mat) = unsafe { (ptr::read(b), ptr::read(test), ptr::read(m)) };
    Bounds::from(src).intersects_obb(&Bounds::from(other), &Matrix4::from(mat), epsilon)
}

/// OBB overlap using the transpose of `*m`.
///
/// # Safety
///
/// `b`, `test`, and `m` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_intersects_obb3x4(
    b: *const bounds,
    test: *const bounds,
    m: *const matrix3,
    epsilon: f32,
) -> bool {
    // SAFETY: the caller guarantees the three inputs are readable.
    let (src, other, mat) = unsafe { (ptr::read(b), ptr::read(test), ptr::read(m)) };
    Bounds::from(src).intersects_obb3x4(&Bounds::from(other), &Matrix3::from(mat), epsilon)
}

/// Minimum distance from `*b` to `*p`.
///
/// # Safety
///
/// `b` and `p` must be readable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn bounds_min_dist(b: *const bounds, p: *const plane) -> f32 {
    // SAFETY: the caller guarantees both pointers are readable.
    let (src, pl) = unsafe { (ptr::read(b), ptr::read(p)) };
    Bounds::from(src).min_dist(&Plane::from(pl))
}
