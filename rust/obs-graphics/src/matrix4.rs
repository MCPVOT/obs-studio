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

    /// `matrix4_from_quat`. A zero norm yields the identity, with `t.w = 1`.
    #[must_use]
    pub fn from_quat(q: Quat) -> Self {
        let norm = q.dot(q);
        let s = if norm > 0.0 { 2.0 / norm } else { 0.0 };
        let xx = q.x * q.x * s;
        let yy = q.y * q.y * s;
        let zz = q.z * q.z * s;
        let xy = q.x * q.y * s;
        let xz = q.x * q.z * s;
        let yz = q.y * q.z * s;
        let wx = q.w * q.x * s;
        let wy = q.w * q.y * s;
        let wz = q.w * q.z * s;
        Self::new(
            Vec4::new(1.0 - (yy + zz), xy + wz, xz - wy, 0.0),
            Vec4::new(xy - wz, 1.0 - (xx + zz), yz + wx, 0.0),
            Vec4::new(xz + wy, yz - wx, 1.0 - (xx + yy), 0.0),
            Vec4::new(0.0, 0.0, 0.0, 1.0),
        )
    }

    /// `matrix4_from_axisang`. Builds the quaternion the way `quat_from_axisang`
    /// does, then [`Self::from_quat`].
    #[must_use]
    pub fn from_axisang(aa: AxisAng) -> Self {
        Self::from_quat(Quat::from_axisang(aa))
    }

    /// `matrix4_from_matrix3`. Copies every lane, then sets `t.w` to 1.
    #[must_use]
    pub fn from_matrix3(m: Matrix3) -> Self {
        Self::new(
            Vec4::new(m.x.x, m.x.y, m.x.z, m.x.w),
            Vec4::new(m.y.x, m.y.y, m.y.z, m.y.w),
            Vec4::new(m.z.x, m.z.y, m.z.z, m.z.w),
            Vec4::new(m.t.x, m.t.y, m.t.z, 1.0),
        )
    }

    /// `matrix4_determinant`. The sign flips each column of row 0, starting
    /// at `+1`. Same parenthesization as `get_3x3_determinant`.
    #[must_use]
    pub fn determinant(self) -> f32 {
        det4(&self.lanes())
    }

    /// `matrix4_translate3v`. Post-multiplies by a translation. `vec3.w` is
    /// ignored; the translation `w` is 1.
    #[must_use]
    pub fn translate3v(self, v: Vec3) -> Self {
        self * &translation(Vec4::from_xyz(v.x, v.y, v.z))
    }

    /// `matrix4_translate4v`. The vector's `w` is kept.
    #[must_use]
    pub fn translate4v(self, v: Vec4) -> Self {
        self * &translation(v)
    }

    /// `matrix4_rotate`. Post-multiplies by the quaternion's matrix.
    #[must_use]
    pub fn rotate(self, q: Quat) -> Self {
        self * &Self::from_quat(q)
    }

    /// `matrix4_rotate_aa`.
    #[must_use]
    pub fn rotate_aa(self, aa: AxisAng) -> Self {
        self * &Self::from_axisang(aa)
    }

    /// `matrix4_scale`. Post-multiplies by a scale matrix. `vec3.w` is ignored.
    #[must_use]
    pub fn scale(self, v: Vec3) -> Self {
        self * &scale_matrix(v)
    }

    /// `matrix4_translate3v_i`. Replaces the translation with `v` transformed
    /// by `self`. `vec3.w` is ignored.
    #[must_use]
    pub fn translate3v_i(self, v: Vec3) -> Self {
        self.translate4v_i(Vec4::from_xyz(v.x, v.y, v.z))
    }

    /// `matrix4_translate4v_i`.
    #[must_use]
    pub fn translate4v_i(self, v: Vec4) -> Self {
        Self::new(self.x, self.y, self.z, v.transform(&self))
    }

    /// `matrix4_rotate_i`. Left-multiplies, then keeps `self.t`, as
    /// `matrix4_mul_4x3_only` does.
    #[must_use]
    pub fn rotate_i(self, q: Quat) -> Self {
        Self::from_quat(q).mul_4x3(&self)
    }

    /// `matrix4_rotate_aa_i`.
    #[must_use]
    pub fn rotate_aa_i(self, aa: AxisAng) -> Self {
        Self::from_axisang(aa).mul_4x3(&self)
    }

    /// `matrix4_scale_i`. Left-multiplies by the scale, then keeps `self.t`.
    #[must_use]
    pub fn scale_i(self, v: Vec3) -> Self {
        scale_matrix(v).mul_4x3(&self)
    }

    /// `matrix4_inv`. `None` when `|det| < 0.0005`: the caller keeps the
    /// previous destination, including every lane.
    #[must_use]
    pub fn try_inv(self) -> Option<Self> {
        let mf = self.lanes();
        let det = det4(&mf);
        if det.abs() < MATRIX4_DET_MIN {
            return None;
        }
        let mut out = [0.0; 16];
        for i in 0..4 {
            for j in 0..4 {
                let sign = 1.0 - (((i + j) % 2) as f32) * 2.0;
                out[j * 4 + i] = det3(&submatrix(&mf, i, j)) * sign / det;
            }
        }
        Some(Self::from_lanes(out))
    }

    /// `matrix4_mul_4x3_only`. The upper three rows are `self * m2`. The
    /// translation row is copied from `m2`. Not exported: libobs gives it
    /// hidden visibility.
    fn mul_4x3(self, m2: &Self) -> Self {
        let product = self * m2;
        Self::new(product.x, product.y, product.z, m2.t)
    }

    fn lanes(self) -> [f32; 16] {
        let rows = [self.x, self.y, self.z, self.t];
        let mut mf = [0.0; 16];
        for (i, row) in rows.iter().enumerate() {
            mf[i * 4] = row.x;
            mf[i * 4 + 1] = row.y;
            mf[i * 4 + 2] = row.z;
            mf[i * 4 + 3] = row.w;
        }
        mf
    }

    fn from_lanes(mf: [f32; 16]) -> Self {
        let row = |i: usize| Vec4::new(mf[i * 4], mf[i * 4 + 1], mf[i * 4 + 2], mf[i * 4 + 3]);
        Self::new(row(0), row(1), row(2), row(3))
    }
}

/// `fabs(det) < 0.0005f` in `matrix4_inv`.
const MATRIX4_DET_MIN: f32 = 0.0005;

impl core::ops::Mul<&Self> for Matrix4 {
    type Output = Self;

    /// `matrix4_mul`. `self` is `m1`. Each output lane is `vec4_dot` of a row
    /// of `m1` with a column of `m2` (a row of `m2.transpose()`).
    fn mul(self, m2: &Self) -> Self {
        let transposed = m2.transpose();
        let row = |r: Vec4| {
            Vec4::new(
                r.dot(transposed.x),
                r.dot(transposed.y),
                r.dot(transposed.z),
                r.dot(transposed.t),
            )
        };
        Self::new(row(self.x), row(self.y), row(self.z), row(self.t))
    }
}

fn translation(v: Vec4) -> Matrix4 {
    Matrix4::new(
        Vec4::new(1.0, 0.0, 0.0, 0.0),
        Vec4::new(0.0, 1.0, 0.0, 0.0),
        Vec4::new(0.0, 0.0, 1.0, 0.0),
        v,
    )
}

fn scale_matrix(v: Vec3) -> Matrix4 {
    Matrix4::new(
        Vec4::new(v.x, 0.0, 0.0, 0.0),
        Vec4::new(0.0, v.y, 0.0, 0.0),
        Vec4::new(0.0, 0.0, v.z, 0.0),
        Vec4::new(0.0, 0.0, 0.0, 1.0),
    )
}

/// Minor of `m` with row `drop_row` and column `drop_col` removed, row-major.
/// Same index walk as `get_3x3_submatrix`.
fn submatrix(mf: &[f32; 16], drop_row: usize, drop_col: usize) -> [f32; 9] {
    let mut dst = [0.0; 9];
    for ti in 0..4 {
        let idst = if ti < drop_row {
            ti
        } else if ti > drop_row {
            ti - 1
        } else {
            continue;
        };
        for tj in 0..4 {
            let jdst = if tj < drop_col {
                tj
            } else if tj > drop_col {
                tj - 1
            } else {
                continue;
            };
            dst[idst * 3 + jdst] = mf[ti * 4 + tj];
        }
    }
    dst
}

/// `get_3x3_determinant`, same parenthesization.
fn det3(m: &[f32; 9]) -> f32 {
    (m[0] * ((m[4] * m[8]) - (m[7] * m[5]))) - (m[1] * ((m[3] * m[8]) - (m[6] * m[5])))
        + (m[2] * ((m[3] * m[7]) - (m[6] * m[4])))
}

/// `matrix4_determinant`. The sign flips each column, starting at `+1`.
fn det4(mf: &[f32; 16]) -> f32 {
    let mut result = 0.0;
    let mut sign = 1.0f32;
    for n in 0..4 {
        result += mf[n] * det3(&submatrix(mf, 0, n)) * sign;
        sign = -sign;
    }
    result
}
