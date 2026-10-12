//! Safe counterpart of `struct plane`. `libobs/graphics/plane.c` is not
//! ported yet; this holds only what the vec3 port needs.

use crate::vec3::Vec3;

/// A plane `dot(dir, v) == dist`, the safe counterpart of `struct plane`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Plane {
    pub dir: Vec3,
    pub dist: f32,
}

impl Plane {
    #[must_use]
    pub const fn new(dir: Vec3, dist: f32) -> Self {
        Self { dir, dist }
    }

    /// Empty stand-in for `plane_from_tri`. The real body replaces this.
    #[must_use]
    pub fn from_tri(_v1: Vec3, _v2: Vec3, _v3: Vec3) -> Self {
        Self::default()
    }

    /// Empty stand-in for `plane_transform`.
    #[must_use]
    pub fn transform(self, _m: &crate::matrix4::Matrix4) -> Self {
        Self::default()
    }

    /// Empty stand-in for `plane_transform3x4`.
    #[must_use]
    pub fn transform3x4(self, _m: &crate::matrix3::Matrix3) -> Self {
        Self::default()
    }

    /// Empty stand-in for `plane_intersection_ray`. `(false, 0.0)` is the
    /// parallel-miss result, so a hit assertion fails.
    #[must_use]
    pub fn intersection_ray(self, _orig: Vec3, _dir: Vec3) -> (bool, f32) {
        (false, 0.0)
    }

    /// Empty stand-in for `plane_intersection_line`. `None` is a miss.
    #[must_use]
    pub fn intersection_line(self, _v1: Vec3, _v2: Vec3) -> Option<f32> {
        None
    }

    /// Empty stand-in for `plane_tri_inside`.
    #[must_use]
    pub fn tri_inside(self, _v1: Vec3, _v2: Vec3, _v3: Vec3, _precision: f32) -> bool {
        false
    }

    /// Empty stand-in for `plane_line_inside`.
    #[must_use]
    pub fn line_inside(self, _v1: Vec3, _v2: Vec3, _precision: f32) -> bool {
        false
    }
}
