//! Safe counterpart of `struct quat`. `libobs/graphics/quat.c` is not
//! ported yet; this holds only what the axisang port needs.

/// A quaternion with vector part (`x`, `y`, `z`) and scalar part `w`, the
/// safe counterpart of `struct quat`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Quat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

impl Quat {
    #[must_use]
    pub const fn new(x: f32, y: f32, z: f32, w: f32) -> Self {
        Self { x, y, z, w }
    }

    /// `quat_mul`.
    #[must_use]
    pub fn mul(self, _other: Self) -> Self {
        todo!()
    }

    /// `quat_from_axisang`.
    #[must_use]
    pub fn from_axisang(_aa: crate::axisang::AxisAng) -> Self {
        todo!()
    }

    /// `quat_from_matrix4`.
    #[must_use]
    pub fn from_matrix4(_m: &crate::matrix4::Matrix4) -> Self {
        todo!()
    }

    /// `quat_from_matrix3`.
    #[must_use]
    pub fn from_matrix3(_m: &crate::matrix3::Matrix3) -> Self {
        todo!()
    }

    /// `quat_get_dir`.
    #[must_use]
    pub fn dir(self) -> crate::vec3::Vec3 {
        todo!()
    }

    /// `quat_set_look_dir`.
    #[must_use]
    pub fn from_look_dir(_dir: crate::vec3::Vec3) -> Self {
        todo!()
    }

    /// `quat_log`.
    #[must_use]
    pub fn log(self) -> Self {
        todo!()
    }

    /// `quat_exp`.
    #[must_use]
    pub fn exp(self) -> Self {
        todo!()
    }

    /// `quat_interpolate`.
    #[must_use]
    pub fn interpolate(self, _q2: Self, _t: f32) -> Self {
        todo!()
    }

    /// `quat_get_tangent`.
    #[must_use]
    pub fn tangent(_prev: Self, _q: Self, _next: Self) -> Self {
        todo!()
    }

    /// `quat_interpolate_cubic`.
    #[must_use]
    pub fn interpolate_cubic(_q1: Self, _q2: Self, _m1: Self, _m2: Self, _t: f32) -> Self {
        todo!()
    }
}
