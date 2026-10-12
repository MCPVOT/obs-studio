//! C ABI shims for the exported functions in `libobs/graphics/vec3.h`.
//!
//! The shims only convert to the safe core in [`crate::vec3`]. Each reads
//! all of its inputs before writing `dst`, so C callers may alias `dst` and
//! an input.

use core::ffi::c_int;
use core::ptr;

use super::math_extra::rand_float;
use super::matrix3::matrix3;
use super::matrix4::matrix4;
use super::plane::plane;
use super::vec4::vec4;
use crate::vec3::Vec3;
use crate::vec4::Vec4;

/// `struct vec3`. Like `struct vec4` it is a union with `__m128`, so it has
/// four floats (`w` is the lane the C code keeps at 0), size 16, align 16.
#[repr(C, align(16))]
#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct vec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl From<vec3> for Vec3 {
    fn from(v: vec3) -> Self {
        Self::with_w(v.x, v.y, v.z, v.w)
    }
}

impl From<Vec3> for vec3 {
    fn from(v: Vec3) -> Self {
        Self {
            x: v.x,
            y: v.y,
            z: v.z,
            w: v.w,
        }
    }
}

/// Writes `x`, `y`, `z` of `*v` with `w` cleared to `*dst`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec4` and `dst` valid for writes of a
/// `vec3`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_from_vec4(dst: *mut vec3, v: *const vec4) {
    // SAFETY: the caller guarantees `v` is readable; read before writing `dst`.
    let src = unsafe { ptr::read(v) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from_vec4(Vec4::from(src)).into()) };
}

/// Returns the signed distance of `*v` from the plane `*p`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec3` and `p` for reads of a `plane`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_plane_dist(v: *const vec3, p: *const plane) -> f32 {
    // SAFETY: the caller guarantees both pointers are readable.
    let (src, pl) = unsafe { (ptr::read(v), ptr::read(p)) };
    Vec3::from(src).plane_dist(&pl.into())
}

/// Writes `*v` rotated by the axes of `*m` to `*dst`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec3`, `m` for reads of a `matrix3`,
/// and `dst` valid for writes of a `vec3`. `dst` may equal `v`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_rotate(dst: *mut vec3, v: *const vec3, m: *const matrix3) {
    // SAFETY: the caller guarantees `v` and `m` are readable; read both
    // before writing `dst`.
    let (src, mat) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from(src).rotate(&mat.into()).into()) };
}

/// Writes the point `*v` transformed by `*m` to `*dst`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec3`, `m` for reads of a `matrix4`,
/// and `dst` valid for writes of a `vec3`. `dst` may equal `v`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_transform(dst: *mut vec3, v: *const vec3, m: *const matrix4) {
    // SAFETY: the caller guarantees `v` and `m` are readable; read both
    // before writing `dst`.
    let (src, mat) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from(src).transform(&mat.into()).into()) };
}

/// Writes `*v` minus the translation of `*m`, rotated by its axes, to
/// `*dst`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec3`, `m` for reads of a `matrix3`,
/// and `dst` valid for writes of a `vec3`. `dst` may equal `v`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_transform3x4(dst: *mut vec3, v: *const vec3, m: *const matrix3) {
    // SAFETY: the caller guarantees `v` and `m` are readable; read both
    // before writing `dst`.
    let (src, mat) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from(src).transform3x4(&mat.into()).into()) };
}

/// Writes the reflection of `*v` through the plane `*p` to `*dst`.
///
/// # Safety
///
/// `v` must be valid for reads of a `vec3`, `p` for reads of a `plane`, and
/// `dst` valid for writes of a `vec3`. `dst` may equal `v`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_mirror(dst: *mut vec3, v: *const vec3, p: *const plane) {
    // SAFETY: the caller guarantees `v` and `p` are readable; read both
    // before writing `dst`.
    let (src, pl) = unsafe { (ptr::read(v), ptr::read(p)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from(src).mirror(&pl.into()).into()) };
}

/// Writes the reflection of `*v` against the direction `*vec` to `*dst`.
///
/// # Safety
///
/// `v` and `vec` must be valid for reads of a `vec3`, and `dst` valid for
/// writes of a `vec3`. `dst` may equal either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_mirrorv(dst: *mut vec3, v: *const vec3, vec: *const vec3) {
    // SAFETY: the caller guarantees both inputs are readable; read them
    // before writing `dst`.
    let (src, dir) = unsafe { (ptr::read(v), ptr::read(vec)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Vec3::from(src).mirrorv(dir.into()).into()) };
}

/// Writes three `rand_float(positive_only)` values to `*dst`, `x` first.
/// `rand_float` is the math-extra port.
///
/// # Safety
///
/// `dst` must be valid for writes of a `vec3`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn vec3_rand(dst: *mut vec3, positive_only: c_int) {
    // SAFETY: `rand_float` only reads the C library's `rand` state.
    let v = Vec3::rand(|| unsafe { rand_float(positive_only) });
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, v.into()) };
}
