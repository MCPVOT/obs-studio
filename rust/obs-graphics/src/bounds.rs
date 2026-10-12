//! Safe core of `libobs/graphics/bounds.c`.
//!
//! `bounds_plane_test` and `bounds_under_plane` build a corner and then dot
//! it with the plane direction. C leaves that corner's `w` uninitialized.
//! These methods set `w` to 0. `bounds_intersects_obb` uses a zero matrix
//! when `matrix4_inv` rejects the input (`|det| < 0.0005`); C leaves that
//! inverse uninitialized.
//!
//! `bounds_get_point` with `i == 8` still writes a corner (`i > 8` is the
//! only reject). A ray or line miss does not produce a `t`.

use crate::matrix3::Matrix3;
use crate::matrix4::Matrix4;
use crate::plane::Plane;
use crate::vec3::Vec3;

/// `M_INFINITE` in `math-defs.h`.
const M_INFINITE: f32 = 3.4e38;
/// `TINY_EPSILON` in `math-defs.h`.
const TINY_EPSILON: f32 = 1e-5;

/// `BOUNDS_OUTSIDE`.
pub const BOUNDS_OUTSIDE: u8 = 1;
/// `BOUNDS_INSIDE`.
pub const BOUNDS_INSIDE: u8 = 2;
/// `BOUNDS_PARTIAL`.
pub const BOUNDS_PARTIAL: u8 = 3;

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

    /// `bounds_move`. `vec3_add` clears `w`.
    #[must_use]
    pub fn move_by(self, v: Vec3) -> Self {
        Self::new(self.min.add_xyz(v), self.max.add_xyz(v))
    }

    /// `bounds_scale`. `vec3_mul` keeps `w`.
    #[must_use]
    pub fn scale(self, v: Vec3) -> Self {
        Self::new(self.min.lane_mul(v), self.max.lane_mul(v))
    }

    /// `bounds_merge`. `vec3_min` / `vec3_max` clear `w`.
    #[must_use]
    pub fn merge(self, other: Self) -> Self {
        Self::new(self.min.min_xyz(other.min), self.max.max_xyz(other.max))
    }

    /// `bounds_merge_point`.
    #[must_use]
    pub fn merge_point(self, v: Vec3) -> Self {
        Self::new(self.min.min_xyz(v), self.max.max_xyz(v))
    }

    /// `bounds_get_point`. `None` when `i > 8`, which is when C writes nothing.
    ///
    /// The returned `w` is 0. C does not write the destination `w`; the shim
    /// leaves it alone.
    #[must_use]
    pub fn point(self, mut i: u32) -> Option<Vec3> {
        if i > 8 {
            return None;
        }
        let x = if i > 3 {
            i -= 4;
            self.max.x
        } else {
            self.min.x
        };
        let y = if i > 1 {
            i -= 2;
            self.max.y
        } else {
            self.min.y
        };
        let z = if i == 1 { self.max.z } else { self.min.z };
        Some(Vec3::new(x, y, z))
    }

    /// `bounds_get_center`. `(max - min) * 0.5 + min`, with `w` cleared.
    #[must_use]
    pub fn center(self) -> Vec3 {
        self.max.sub_xyz(self.min).mulf(0.5).add_xyz(self.min)
    }

    /// `bounds_transform`. Corners 0..7, grown into an AABB with scalar `<` / `>`.
    #[must_use]
    pub fn transform(self, m: &Matrix4) -> Self {
        self.map_corners(|p| p.transform(m))
    }

    /// `bounds_transform3x4`.
    #[must_use]
    pub fn transform3x4(self, m: &Matrix3) -> Self {
        self.map_corners(|p| p.transform3x4(m))
    }

    /// `bounds_intersection_ray`. `None` is a miss, and C does not write `*t`.
    #[must_use]
    pub fn intersection_ray(self, orig: Vec3, dir: Vec3) -> Option<f32> {
        let mut t_max = M_INFINITE;
        let mut t_min = -M_INFINITE;
        let center = self.center();
        let max_offset = self.max.sub_xyz(center);
        let box_offset = center.sub_xyz(orig);
        let e = [box_offset.x, box_offset.y, box_offset.z];
        let f = [dir.x, dir.y, dir.z];
        let extent = [max_offset.x, max_offset.y, max_offset.z];

        for i in 0..3 {
            if f[i].abs() > 0.0 {
                let fi = 1.0 / f[i];
                let t1 = (e[i] + extent[i]) * fi;
                let t2 = (e[i] - extent[i]) * fi;
                let (near, far) = if t1 > t2 { (t2, t1) } else { (t1, t2) };
                if near > t_min {
                    t_min = near;
                }
                if far < t_max {
                    t_max = far;
                }
                if t_min > t_max || t_max < 0.0 {
                    return None;
                }
            } else if (-e[i] - extent[i]) > 0.0 || (-e[i] + extent[i]) < 0.0 {
                return None;
            }
        }
        Some(if t_min > 0.0 { t_min } else { t_max })
    }

    /// `bounds_intersection_line`. The ray `t` is divided by the segment length.
    /// `None` leaves C `*t` unchanged.
    #[must_use]
    pub fn intersection_line(self, p1: Vec3, p2: Vec3) -> Option<f32> {
        let dir = p2.sub_xyz(p1);
        let length = dir.len();
        if length <= TINY_EPSILON {
            return None;
        }
        let t = self.intersection_ray(p1, dir.mulf(1.0 / length))?;
        Some(t / length)
    }

    /// `bounds_plane_test`, with corner `w` set to 0.
    ///
    /// Returns [`BOUNDS_OUTSIDE`], [`BOUNDS_INSIDE`], or [`BOUNDS_PARTIAL`].
    #[must_use]
    pub fn plane_test(self, p: &Plane) -> u8 {
        let dir = [p.dir.x, p.dir.y, p.dir.z];
        let bmin = [self.min.x, self.min.y, self.min.z];
        let bmax = [self.max.x, self.max.y, self.max.z];
        let mut near = [0.0; 3];
        let mut far = [0.0; 3];
        for i in 0..3 {
            if dir[i] >= 0.0 {
                near[i] = bmin[i];
                far[i] = bmax[i];
            } else {
                near[i] = bmax[i];
                far[i] = bmin[i];
            }
        }
        let near = Vec3::new(near[0], near[1], near[2]);
        let far = Vec3::new(far[0], far[1], far[2]);
        if near.plane_dist(p) > 0.0 {
            BOUNDS_OUTSIDE
        } else if far.plane_dist(p) >= 0.0 {
            BOUNDS_PARTIAL
        } else {
            BOUNDS_INSIDE
        }
    }

    /// `bounds_under_plane`, with the corner `w` set to 0.
    #[must_use]
    pub fn under_plane(self, p: &Plane) -> bool {
        let vmin = Vec3::new(
            if p.dir.x < 0.0 {
                self.max.x
            } else {
                self.min.x
            },
            if p.dir.y < 0.0 {
                self.max.y
            } else {
                self.min.y
            },
            if p.dir.z < 0.0 {
                self.max.z
            } else {
                self.min.z
            },
        );
        vmin.dot(p.dir) <= p.dist
    }

    /// `bounds_intersects`. Touching at `epsilon` counts (`<=`).
    #[must_use]
    pub fn intersects(self, test: &Self, epsilon: f32) -> bool {
        (self.min.x - test.max.x) <= epsilon
            && (test.min.x - self.max.x) <= epsilon
            && (self.min.y - test.max.y) <= epsilon
            && (test.min.y - self.max.y) <= epsilon
            && (self.min.z - test.max.z) <= epsilon
            && (test.min.z - self.max.z) <= epsilon
    }

    /// `bounds_intersects_obb`. A rejected inverse uses a zero matrix.
    #[must_use]
    pub fn intersects_obb(self, test: &Self, m: &Matrix4, epsilon: f32) -> bool {
        let m_inv = m.try_inv().unwrap_or_default();
        let b_tr = self.transform(m);
        let test_tr = test.transform(&m_inv);
        self.intersects(&test_tr, epsilon) && b_tr.intersects(test, epsilon)
    }

    /// `bounds_intersects_obb3x4`. The "inverse" is the transpose.
    #[must_use]
    pub fn intersects_obb3x4(self, test: &Self, m: &Matrix3, epsilon: f32) -> bool {
        let m_inv = m.transpose();
        let b_tr = self.transform3x4(m);
        let test_tr = test.transform3x4(&m_inv);
        self.intersects(&test_tr, epsilon) && b_tr.intersects(test, epsilon)
    }

    /// `bounds_min_dist`. `p.dist` cancels, leaving `dot(center, dir)` minus
    /// half the projected extent. `dir.w` is not read: the center's `w` is 0
    /// and `vec3_abs` clears the direction's `w`.
    #[must_use]
    pub fn min_dist(self, p: &Plane) -> f32 {
        let vec_len = self.max.sub_xyz(self.min).dot(p.dir.abs_xyz()) * 0.5;
        p.dist + self.center().plane_dist(p) - vec_len
    }

    /// Grow an AABB around the eight transformed corners. The first corner is
    /// copied, including `w`. Later corners update only x, y, and z, and a
    /// comparison with NaN does not update.
    fn map_corners(self, mut f: impl FnMut(Vec3) -> Vec3) -> Self {
        let mut temp_min = Vec3::new(0.0, 0.0, 0.0);
        let mut temp_max = Vec3::new(0.0, 0.0, 0.0);
        let mut init = false;
        for i in 0..8 {
            let p = f(self.point(i).expect("corners 0..7 are written"));
            if !init {
                temp_min = p;
                temp_max = p;
                init = true;
            } else {
                if p.x < temp_min.x {
                    temp_min.x = p.x;
                } else if p.x > temp_max.x {
                    temp_max.x = p.x;
                }
                if p.y < temp_min.y {
                    temp_min.y = p.y;
                } else if p.y > temp_max.y {
                    temp_max.y = p.y;
                }
                if p.z < temp_min.z {
                    temp_min.z = p.z;
                } else if p.z > temp_max.z {
                    temp_max.z = p.z;
                }
            }
        }
        Self::new(temp_min, temp_max)
    }
}
