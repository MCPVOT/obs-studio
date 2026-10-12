//! Safe core of `libobs/graphics/bounds.c`.
//!
//! Empty stubs. The GREEN commit replaces these bodies.

use crate::matrix3::Matrix3;
use crate::matrix4::Matrix4;
use crate::plane::Plane;
use crate::vec3::Vec3;

/// An axis-aligned box, the safe counterpart of `struct bounds`.
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct Bounds {
    pub min: Vec3,
    pub max: Vec3,
}

impl Bounds {
    #[must_use]
    pub const fn new(min: Vec3, max: Vec3) -> Self {
        Self { min, max }
    }

    /// `bounds_move`.
    #[must_use]
    pub fn move_by(self, _v: Vec3) -> Self {
        Self::default()
    }

    /// `bounds_scale`.
    #[must_use]
    pub fn scale(self, _v: Vec3) -> Self {
        Self::default()
    }

    /// `bounds_merge`.
    #[must_use]
    pub fn merge(self, _other: Self) -> Self {
        Self::default()
    }

    /// `bounds_merge_point`.
    #[must_use]
    pub fn merge_point(self, _v: Vec3) -> Self {
        Self::default()
    }

    /// `bounds_get_point`. `None` when `i > 8` (C writes nothing).
    #[must_use]
    pub fn point(self, _i: u32) -> Option<Vec3> {
        None
    }

    /// `bounds_get_center`.
    #[must_use]
    pub fn center(self) -> Vec3 {
        Vec3::new(0.0, 0.0, 0.0)
    }

    /// `bounds_transform`.
    #[must_use]
    pub fn transform(self, _m: &Matrix4) -> Self {
        Self::default()
    }

    /// `bounds_transform3x4`.
    #[must_use]
    pub fn transform3x4(self, _m: &Matrix3) -> Self {
        Self::default()
    }

    /// `bounds_intersection_ray`. `None` leaves C `*t` unchanged.
    #[must_use]
    pub fn intersection_ray(self, _orig: Vec3, _dir: Vec3) -> Option<f32> {
        None
    }

    /// `bounds_intersection_line`. `None` leaves C `*t` unchanged.
    #[must_use]
    pub fn intersection_line(self, _p1: Vec3, _p2: Vec3) -> Option<f32> {
        None
    }

    /// `bounds_plane_test`. `1` outside, `2` inside, `3` partial.
    ///
    /// The corner `w` lanes are 0. C leaves them uninitialized.
    #[must_use]
    pub fn plane_test(self, _p: &Plane) -> u8 {
        0
    }

    /// `bounds_under_plane`. The corner `w` lane is 0. C leaves it uninitialized.
    #[must_use]
    pub fn under_plane(self, _p: &Plane) -> bool {
        false
    }

    /// `bounds_intersects`.
    #[must_use]
    pub fn intersects(self, _test: &Self, _epsilon: f32) -> bool {
        false
    }

    /// `bounds_intersects_obb`. A rejected inverse (`|det| < 0.0005`) uses a
    /// zero matrix. C leaves that inverse uninitialized.
    #[must_use]
    pub fn intersects_obb(self, _test: &Self, _m: &Matrix4, _epsilon: f32) -> bool {
        false
    }

    /// `bounds_intersects_obb3x4`.
    #[must_use]
    pub fn intersects_obb3x4(self, _test: &Self, _m: &Matrix3, _epsilon: f32) -> bool {
        false
    }

    /// `bounds_min_dist`.
    #[must_use]
    pub fn min_dist(self, _p: &Plane) -> f32 {
        0.0
    }
}
