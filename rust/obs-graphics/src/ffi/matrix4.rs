//! C ABI shims for the exported functions in `libobs/graphics/matrix4.h`.
//!
//! The shims only convert to the safe core in [`crate::matrix4`]. Each reads
//! its inputs before writing `dst`, so a caller may alias `dst` with an input.
//! Header-inline helpers (`matrix4_copy`, `matrix4_identity`, `matrix4_translate3f`,
//! `matrix4_rotate_aa4f`, `matrix4_scale3f`) stay in the C header.
//! `matrix4_mul_4x3_only` stays out of the export list: the C file gives it
//! hidden visibility.

use core::ptr;

use super::axisang::axisang;
use super::matrix3::matrix3;
use super::quat::quat;
use super::vec3::vec3;
use super::vec4::vec4;
use crate::axisang::AxisAng;
use crate::matrix4::Matrix4;
use crate::quat::Quat;
use crate::vec3::Vec3;
use crate::vec4::Vec4;

/// `struct matrix4`: four `vec4` rows, size 64, align 16.
#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct matrix4 {
    pub x: vec4,
    pub y: vec4,
    pub z: vec4,
    pub t: vec4,
}

impl From<matrix4> for Matrix4 {
    fn from(m: matrix4) -> Self {
        Self::new(m.x.into(), m.y.into(), m.z.into(), m.t.into())
    }
}

impl From<Matrix4> for matrix4 {
    fn from(m: Matrix4) -> Self {
        Self {
            x: m.x.into(),
            y: m.y.into(),
            z: m.z.into(),
            t: m.t.into(),
        }
    }
}

/// Writes `*m` as a `matrix4`, with `t.w` set to 1.
///
/// # Safety
///
/// `m` must be valid for reads of a `matrix3` and `dst` valid for writes of
/// a `matrix4`. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_from_matrix3(dst: *mut matrix4, m: *const matrix3) {
    // SAFETY: the caller guarantees `m` is readable; read before writing `dst`.
    let src = unsafe { ptr::read(m) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from_matrix3(src.into()).into()) };
}

/// Writes the rotation matrix of `*q` to `*dst`.
///
/// # Safety
///
/// `q` must be valid for reads of a `quat` and `dst` valid for writes of a
/// `matrix4`. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_from_quat(dst: *mut matrix4, q: *const quat) {
    // SAFETY: the caller guarantees `q` is readable; read before writing `dst`.
    let src = unsafe { ptr::read(q) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from_quat(Quat::from(src)).into()) };
}

/// Writes the rotation matrix of `*aa` to `*dst`.
///
/// # Safety
///
/// `aa` must be valid for reads of an `axisang` and `dst` valid for writes
/// of a `matrix4`. They may overlap.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_from_axisang(dst: *mut matrix4, aa: *const axisang) {
    // SAFETY: the caller guarantees `aa` is readable; read before writing `dst`.
    let src = unsafe { ptr::read(aa) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from_axisang(AxisAng::from(src)).into()) };
}

/// Writes `m1 * m2` to `*dst`.
///
/// # Safety
///
/// `m1` and `m2` must be valid for reads of a `matrix4` and `dst` valid for
/// writes. `dst` may alias either input.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_mul(dst: *mut matrix4, m1: *const matrix4, m2: *const matrix4) {
    // SAFETY: the caller guarantees both inputs are readable; read before writing.
    let (a, b) = unsafe { (ptr::read(m1), ptr::read(m2)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, (Matrix4::from(a) * &Matrix4::from(b)).into()) };
}

/// Returns the determinant of `*m`.
///
/// # Safety
///
/// `m` must be valid for reads of a `matrix4`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_determinant(m: *const matrix4) -> f32 {
    // SAFETY: the caller guarantees `m` is readable.
    let src = unsafe { ptr::read(m) };
    Matrix4::from(src).determinant()
}

/// Writes `*m` translated by `*v` to `*dst`.
///
/// # Safety
///
/// `m` and `v` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_translate3v(dst: *mut matrix4, m: *const matrix4, v: *const vec3) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (src, offset) = unsafe { (ptr::read(m), ptr::read(v)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(
            dst,
            Matrix4::from(src).translate3v(Vec3::from(offset)).into(),
        );
    }
}

/// Writes `*m` translated by `*v` to `*dst`.
///
/// # Safety
///
/// `m` and `v` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_translate4v(dst: *mut matrix4, m: *const matrix4, v: *const vec4) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (src, offset) = unsafe { (ptr::read(m), ptr::read(v)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(
            dst,
            Matrix4::from(src).translate4v(Vec4::from(offset)).into(),
        );
    }
}

/// Writes `*m` rotated by `*q` to `*dst`.
///
/// # Safety
///
/// `m` and `q` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_rotate(dst: *mut matrix4, m: *const matrix4, q: *const quat) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (src, rot) = unsafe { (ptr::read(m), ptr::read(q)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from(src).rotate(Quat::from(rot)).into()) };
}

/// Writes `*m` rotated by `*aa` to `*dst`.
///
/// # Safety
///
/// `m` and `aa` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_rotate_aa(
    dst: *mut matrix4,
    m: *const matrix4,
    aa: *const axisang,
) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (src, rot) = unsafe { (ptr::read(m), ptr::read(aa)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(dst, Matrix4::from(src).rotate_aa(AxisAng::from(rot)).into());
    }
}

/// Writes `*m` scaled by `*v` to `*dst`.
///
/// # Safety
///
/// `m` and `v` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_scale(dst: *mut matrix4, m: *const matrix4, v: *const vec3) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (src, scale) = unsafe { (ptr::read(m), ptr::read(v)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from(src).scale(Vec3::from(scale)).into()) };
}

/// Writes `*v` translated by `*m` to `*dst`, keeping `*m`'s axes.
///
/// # Safety
///
/// `v` and `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_translate3v_i(
    dst: *mut matrix4,
    v: *const vec3,
    m: *const matrix4,
) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (offset, src) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(
            dst,
            Matrix4::from(src).translate3v_i(Vec3::from(offset)).into(),
        );
    }
}

/// Writes `*v` translated by `*m` to `*dst`, keeping `*m`'s axes.
///
/// # Safety
///
/// `v` and `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_translate4v_i(
    dst: *mut matrix4,
    v: *const vec4,
    m: *const matrix4,
) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (offset, src) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(
            dst,
            Matrix4::from(src).translate4v_i(Vec4::from(offset)).into(),
        );
    }
}

/// Writes `*q` applied on the left of `*m` to `*dst`. The translation row
/// stays `m->t`.
///
/// # Safety
///
/// `q` and `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_rotate_i(dst: *mut matrix4, q: *const quat, m: *const matrix4) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (rot, src) = unsafe { (ptr::read(q), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from(src).rotate_i(Quat::from(rot)).into()) };
}

/// Writes `*aa` applied on the left of `*m` to `*dst`. The translation row
/// stays `m->t`.
///
/// # Safety
///
/// `aa` and `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_rotate_aa_i(
    dst: *mut matrix4,
    aa: *const axisang,
    m: *const matrix4,
) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (rot, src) = unsafe { (ptr::read(aa), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe {
        ptr::write(
            dst,
            Matrix4::from(src).rotate_aa_i(AxisAng::from(rot)).into(),
        );
    }
}

/// Writes `*v` applied on the left of `*m` to `*dst`. The translation row
/// stays `m->t`.
///
/// # Safety
///
/// `v` and `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_scale_i(dst: *mut matrix4, v: *const vec3, m: *const matrix4) {
    // SAFETY: the caller guarantees the inputs are readable; read before writing.
    let (scale, src) = unsafe { (ptr::read(v), ptr::read(m)) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from(src).scale_i(Vec3::from(scale)).into()) };
}

/// Writes the inverse of `*m` to `*dst`. Returns false, and leaves `*dst`
/// unchanged, when `|det| < 0.0005`.
///
/// # Safety
///
/// `m` must be readable and `dst` readable and writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_inv(dst: *mut matrix4, m: *const matrix4) -> bool {
    // SAFETY: the caller guarantees `m` is readable; read before any write.
    let src = unsafe { ptr::read(m) };
    match Matrix4::from(src).try_inv() {
        Some(out) => {
            // SAFETY: the caller guarantees `dst` is writable.
            unsafe { ptr::write(dst, out.into()) };
            true
        }
        None => false,
    }
}

/// Writes the transpose of `*m` to `*dst`.
///
/// # Safety
///
/// `m` must be readable and `dst` writable. `dst` may alias `m`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn matrix4_transpose(dst: *mut matrix4, m: *const matrix4) {
    // SAFETY: the caller guarantees `m` is readable; read before writing `dst`.
    let src = unsafe { ptr::read(m) };
    // SAFETY: the caller guarantees `dst` is writable.
    unsafe { ptr::write(dst, Matrix4::from(src).transpose().into()) };
}
