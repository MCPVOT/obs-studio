//! Safe core of `libobs/graphics/plane.c`.
//!
//! Header-inline helpers in `plane.h` stay C. `from_tri` takes its vertices
//! by value, so it dots the original `v1`. The C shim special-cases
//! `v1 == &dst->dir`, where C overwrites that vector before the dot.

use crate::matrix3::Matrix3;
use crate::matrix4::Matrix4;
use crate::vec3::Vec3;

/// `EPSILON` from `libobs/graphics/math-defs.h`.
const EPSILON: f32 = 1e-4;

/// `close_float` from `math-defs.h`.
fn close_float(f1: f32, f2: f32, precision: f32) -> bool {
    (f1 - f2).abs() <= precision
}

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

    /// Normal of the triangle `v1, v2, v3`, with `dist` from `v1`
    /// (`plane_from_tri`). A zero-area triangle yields a zero normal.
    #[must_use]
    pub fn from_tri(v1: Vec3, v2: Vec3, v3: Vec3) -> Self {
        let temp = v2.sub_xyz(v1);
        let dir = temp.cross(v3.sub_xyz(v1)).norm();
        Self::new(dir, v1.dot(dir))
    }

    /// `plane_transform`. The normal goes through `vec3_transform`, so it is
    /// moved as a point, then normalized. `dist` subtracts the transformed
    /// origin.
    #[must_use]
    pub fn transform(self, m: &Matrix4) -> Self {
        let dir = self.dir.transform(m).norm();
        let origin = Vec3::new(0.0, 0.0, 0.0).transform(m);
        Self::new(dir, self.dist - dir.dot(origin))
    }

    /// `plane_transform3x4`. Subtracts `m.t` from the normal before rotating.
    #[must_use]
    pub fn transform3x4(self, m: &Matrix3) -> Self {
        let dir = self.dir.transform3x4(m).norm();
        let origin = m.t.transform3x4(m);
        Self::new(dir, self.dist - dir.dot(origin))
    }

    /// `plane_intersection_ray`. A parallel ray (`|dir · normal| < EPSILON`)
    /// is a miss and returns `t = 0`. A hit behind the origin returns a
    /// negative `t` and `true`.
    #[must_use]
    pub fn intersection_ray(self, orig: Vec3, dir: Vec3) -> (bool, f32) {
        let c = self.dir.dot(dir);
        if c.abs() < EPSILON {
            (false, 0.0)
        } else {
            (true, (self.dist - self.dir.dot(orig)) / c)
        }
    }

    /// `plane_intersection_line`. `None` is a miss, and the shim then leaves
    /// the caller's `*t` unchanged. Both endpoints on the plane is a miss.
    #[must_use]
    pub fn intersection_line(self, v1: Vec3, v2: Vec3) -> Option<f32> {
        let p1_dist = v1.plane_dist(&self);
        let p2_dist = v2.plane_dist(&self);
        if close_float(p1_dist, 0.0, EPSILON) {
            if close_float(p2_dist, 0.0, EPSILON) {
                return None;
            }
            return Some(0.0);
        } else if close_float(p2_dist, 0.0, EPSILON) {
            return Some(1.0);
        }

        if (p1_dist > 0.0) == (p2_dist > 0.0) {
            return None;
        }
        let p1_abs = p1_dist.abs();
        let dist2 = p1_abs + p2_dist.abs();
        if dist2 < EPSILON {
            return None;
        }
        Some(p1_abs / dist2)
    }

    /// `plane_tri_inside`. True when any vertex is outside the open slab
    /// `(-precision, precision)`.
    #[must_use]
    pub fn tri_inside(self, v1: Vec3, v2: Vec3, v3: Vec3, precision: f32) -> bool {
        self.sides(&[v1, v2, v3], precision)
    }

    /// `plane_line_inside`. Same slab test as [`Self::tri_inside`].
    #[must_use]
    pub fn line_inside(self, v1: Vec3, v2: Vec3, precision: f32) -> bool {
        self.sides(&[v1, v2], precision)
    }

    fn sides(self, points: &[Vec3], precision: f32) -> bool {
        let mut sides = 0;
        for point in points {
            let d = point.plane_dist(&self);
            if d >= precision {
                sides |= 2;
            } else if d <= -precision {
                sides |= 1;
            }
        }
        sides != 0
    }
}
