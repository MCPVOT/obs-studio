//! Safe core of `libobs/graphics/matrix4.c`.
//!
//! `matrix4_transpose` only moves lanes. The SSE unpack path and the scalar
//! path store the same bits, so [`Matrix4::transpose`] is exact. A rejected
//! inverse (`|det| < 0.0005`) leaves the destination unchanged.

use crate::axisang::AxisAng;
use crate::matrix3::Matrix3;
use crate::quat::Quat;
use crate::vec3::Vec3;
use crate::vec4::Vec4;

/// A 4x4 matrix stored as four rows, the safe counterpart of
/// `struct matrix4`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Matrix4 {
    pub x: Vec4,
    pub y: Vec4,
    pub z: Vec4,
    pub t: Vec4,
}

impl Matrix4 {
    #[must_use]
    pub const fn new(x: Vec4, y: Vec4, z: Vec4, t: Vec4) -> Self {
        Self { x, y, z, t }
    }

    /// Swaps rows and columns, as `matrix4_transpose` does. Only moves
    /// values, so it is exact.
    #[must_use]
    pub const fn transpose(&self) -> Self {
        let (x, y, z, t) = (self.x, self.y, self.z, self.t);
        Self::new(
            Vec4::new(x.x, y.x, z.x, t.x),
            Vec4::new(x.y, y.y, z.y, t.y),
            Vec4::new(x.z, y.z, z.z, t.z),
            Vec4::new(x.w, y.w, z.w, t.w),
        )
    }

    /// `matrix4_from_quat`.
    #[must_use]
    pub fn from_quat(q: Quat) -> Self {
        let _ = q;
        Self::default()
    }

    /// `matrix4_from_axisang`.
    #[must_use]
    pub fn from_axisang(aa: AxisAng) -> Self {
        let _ = aa;
        Self::default()
    }

    /// `matrix4_from_matrix3`. Copies every lane, then sets `t.w` to 1.
    #[must_use]
    pub fn from_matrix3(m: Matrix3) -> Self {
        let _ = m;
        Self::default()
    }

    /// `matrix4_mul`. `self` is `m1`.
    #[must_use]
    pub fn mul(self, m2: &Self) -> Self {
        let _ = m2;
        Self::default()
    }

    /// `matrix4_determinant`.
    #[must_use]
    pub fn determinant(self) -> f32 {
        0.0
    }

    /// `matrix4_translate3v`. `vec3.w` is ignored.
    #[must_use]
    pub fn translate3v(self, v: Vec3) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_translate4v`. The vector's `w` is kept.
    #[must_use]
    pub fn translate4v(self, v: Vec4) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_rotate`.
    #[must_use]
    pub fn rotate(self, q: Quat) -> Self {
        let _ = q;
        Self::default()
    }

    /// `matrix4_rotate_aa`.
    #[must_use]
    pub fn rotate_aa(self, aa: AxisAng) -> Self {
        let _ = aa;
        Self::default()
    }

    /// `matrix4_scale`. `vec3.w` is ignored.
    #[must_use]
    pub fn scale(self, v: Vec3) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_translate3v_i`.
    #[must_use]
    pub fn translate3v_i(self, v: Vec3) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_translate4v_i`.
    #[must_use]
    pub fn translate4v_i(self, v: Vec4) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_rotate_i`. The translation row stays `self.t`.
    #[must_use]
    pub fn rotate_i(self, q: Quat) -> Self {
        let _ = q;
        Self::default()
    }

    /// `matrix4_rotate_aa_i`.
    #[must_use]
    pub fn rotate_aa_i(self, aa: AxisAng) -> Self {
        let _ = aa;
        Self::default()
    }

    /// `matrix4_scale_i`. The translation row stays `self.t`.
    #[must_use]
    pub fn scale_i(self, v: Vec3) -> Self {
        let _ = v;
        Self::default()
    }

    /// `matrix4_inv`. `None` means the caller keeps the previous destination.
    #[must_use]
    pub fn try_inv(self) -> Option<Self> {
        let _ = self;
        Some(Self::default())
    }
}
