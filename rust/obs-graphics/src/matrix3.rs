//! Safe core of `libobs/graphics/matrix3.c`.
//!
//! `struct matrix3` stores four `vec3` rows, each with an SSE `w` lane.
//! `matrix3_transpose` copies `z.w` into the `w` lane of every axis row, and
//! `matrix3_inv` leaves `dst` unchanged except `t.w = 0` when
//! `matrix4_inv` rejects the determinant. Both are matched exactly.

use crate::axisang::AxisAng;
use crate::matrix4::Matrix4;
use crate::plane::Plane;
use crate::quat::Quat;
use crate::vec3::Vec3;

/// Three axes and a translation, the safe counterpart of `struct matrix3`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Matrix3 {
    pub x: Vec3,
    pub y: Vec3,
    pub z: Vec3,
    pub t: Vec3,
}

impl Matrix3 {
    #[must_use]
    pub const fn new(x: Vec3, y: Vec3, z: Vec3, t: Vec3) -> Self {
        Self { x, y, z, t }
    }

    /// `matrix3_from_quat`.
    #[must_use]
    pub fn from_quat(q: Quat) -> Self {
        let _ = q;
        todo!()
    }

    /// `matrix3_from_axisang`.
    #[must_use]
    pub fn from_axisang(aa: AxisAng) -> Self {
        let _ = aa;
        todo!()
    }

    /// `matrix3_from_matrix4`.
    #[must_use]
    pub fn from_matrix4(m: Matrix4) -> Self {
        let _ = m;
        todo!()
    }

    /// `matrix3_mul`: rows of `self` transformed by `m2`.
    #[must_use]
    pub fn mul(self, m2: &Self) -> Self {
        let _ = m2;
        todo!()
    }

    /// `matrix3_rotate`.
    #[must_use]
    pub fn rotate(self, q: Quat) -> Self {
        let _ = q;
        todo!()
    }

    /// `matrix3_rotate_aa`.
    #[must_use]
    pub fn rotate_aa(self, aa: AxisAng) -> Self {
        let _ = aa;
        todo!()
    }

    /// `matrix3_scale`.
    #[must_use]
    pub fn scale(self, v: Vec3) -> Self {
        let _ = v;
        todo!()
    }

    /// `matrix3_transpose`.
    #[must_use]
    pub fn transpose(self) -> Self {
        todo!()
    }

    /// `matrix3_inv`. `self` is the previous `dst`. A rejected determinant
    /// keeps `self` and clears `t.w`.
    #[must_use]
    pub fn inv(self, m: Self) -> Self {
        let _ = m;
        todo!()
    }

    /// `matrix3_mirror`.
    #[must_use]
    pub fn mirror(self, p: &Plane) -> Self {
        let _ = p;
        todo!()
    }

    /// `matrix3_mirrorv`.
    #[must_use]
    pub fn mirrorv(self, v: Vec3) -> Self {
        let _ = v;
        todo!()
    }
}
