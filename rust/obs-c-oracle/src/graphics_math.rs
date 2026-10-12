//! The graphics math cluster (vec3, vec4, matrix3, matrix4, quat, plane,
//! bounds, axisang, math-extra). The C files call each other, so all of
//! them are compiled; only the functions a port needs are declared here.

use core::ffi::c_int;

use super::vec2::OracleVec2;

/// Independent declaration of `struct vec3`: four floats in a union
/// with `__m128`, so 16 bytes aligned to 16.
#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleVec3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// Independent declaration of `struct vec4`.
#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleVec4 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// Independent declaration of `struct matrix4`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleMatrix4 {
    pub x: OracleVec4,
    pub y: OracleVec4,
    pub z: OracleVec4,
    pub t: OracleVec4,
}

/// Independent declaration of `struct plane`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OraclePlane {
    pub dir: OracleVec3,
    pub dist: f32,
}

/// Independent declaration of `struct matrix3`.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleMatrix3 {
    pub x: OracleVec3,
    pub y: OracleVec3,
    pub z: OracleVec3,
    pub t: OracleVec3,
}

/// Independent declaration of `struct axisang` (no `__m128` member).
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleAxisAng {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

/// Independent declaration of `struct quat`.
#[repr(C, align(16))]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleQuat {
    pub x: f32,
    pub y: f32,
    pub z: f32,
    pub w: f32,
}

unsafe extern "C" {
    pub fn oracle_axisang_from_quat(dst: *mut OracleAxisAng, q: *const OracleQuat);

    pub fn oracle_axisang_size() -> usize;
    pub fn oracle_axisang_align() -> usize;
    pub fn oracle_axisang_offset_x() -> usize;
    pub fn oracle_axisang_offset_y() -> usize;
    pub fn oracle_axisang_offset_z() -> usize;
    pub fn oracle_axisang_offset_w() -> usize;
    pub fn oracle_axisang_offset_ptr() -> usize;
    pub fn oracle_quat_size() -> usize;
    pub fn oracle_quat_align() -> usize;
    pub fn oracle_quat_offset_x() -> usize;
    pub fn oracle_quat_offset_y() -> usize;
    pub fn oracle_quat_offset_z() -> usize;
    pub fn oracle_quat_offset_w() -> usize;
    pub fn oracle_quat_offset_m() -> usize;

    pub fn oracle_quat_mul(dst: *mut OracleQuat, q1: *const OracleQuat, q2: *const OracleQuat);
    pub fn oracle_quat_from_axisang(dst: *mut OracleQuat, aa: *const OracleAxisAng);
    pub fn oracle_quat_from_matrix3(dst: *mut OracleQuat, m: *const OracleMatrix3);
    pub fn oracle_quat_from_matrix4(dst: *mut OracleQuat, m: *const OracleMatrix4);
    pub fn oracle_quat_get_dir(dst: *mut OracleVec3, q: *const OracleQuat);
    pub fn oracle_quat_set_look_dir(dst: *mut OracleQuat, dir: *const OracleVec3);
    pub fn oracle_quat_log(dst: *mut OracleQuat, q: *const OracleQuat);
    pub fn oracle_quat_exp(dst: *mut OracleQuat, q: *const OracleQuat);
    pub fn oracle_quat_interpolate(
        dst: *mut OracleQuat,
        q1: *const OracleQuat,
        q2: *const OracleQuat,
        t: f32,
    );
    pub fn oracle_quat_get_tangent(
        dst: *mut OracleQuat,
        prev: *const OracleQuat,
        q: *const OracleQuat,
        next: *const OracleQuat,
    );
    pub fn oracle_quat_interpolate_cubic(
        dst: *mut OracleQuat,
        q1: *const OracleQuat,
        q2: *const OracleQuat,
        m1: *const OracleQuat,
        m2: *const OracleQuat,
        t: f32,
    );
}

unsafe extern "C" {
    pub fn oracle_vec3_from_vec4(dst: *mut OracleVec3, v: *const OracleVec4);
    pub fn oracle_vec3_plane_dist(v: *const OracleVec3, p: *const OraclePlane) -> f32;
    pub fn oracle_vec3_rotate(dst: *mut OracleVec3, v: *const OracleVec3, m: *const OracleMatrix3);
    pub fn oracle_vec3_transform(
        dst: *mut OracleVec3,
        v: *const OracleVec3,
        m: *const OracleMatrix4,
    );
    pub fn oracle_vec3_transform3x4(
        dst: *mut OracleVec3,
        v: *const OracleVec3,
        m: *const OracleMatrix3,
    );
    pub fn oracle_vec3_mirror(dst: *mut OracleVec3, v: *const OracleVec3, p: *const OraclePlane);
    pub fn oracle_vec3_mirrorv(dst: *mut OracleVec3, v: *const OracleVec3, vec: *const OracleVec3);
    pub fn oracle_vec3_rand(dst: *mut OracleVec3, positive_only: c_int);

    pub fn oracle_vec3_offset_x() -> usize;
    pub fn oracle_vec3_offset_y() -> usize;
    pub fn oracle_vec3_offset_z() -> usize;
    pub fn oracle_vec3_offset_w() -> usize;
    pub fn oracle_vec3_offset_ptr() -> usize;
    pub fn oracle_vec3_offset_m() -> usize;
    pub fn oracle_plane_size() -> usize;
    pub fn oracle_plane_align() -> usize;
    pub fn oracle_plane_offset_dir() -> usize;
    pub fn oracle_plane_offset_dist() -> usize;
    pub fn oracle_matrix3_size() -> usize;
    pub fn oracle_matrix3_align() -> usize;
    pub fn oracle_matrix3_offset_x() -> usize;
    pub fn oracle_matrix3_offset_y() -> usize;
    pub fn oracle_matrix3_offset_z() -> usize;
    pub fn oracle_matrix3_offset_t() -> usize;

    pub fn oracle_matrix3_from_quat(dst: *mut OracleMatrix3, q: *const OracleQuat);
    pub fn oracle_matrix3_from_axisang(dst: *mut OracleMatrix3, aa: *const OracleAxisAng);
    pub fn oracle_matrix3_from_matrix4(dst: *mut OracleMatrix3, m: *const OracleMatrix4);
    pub fn oracle_matrix3_mul(
        dst: *mut OracleMatrix3,
        m1: *const OracleMatrix3,
        m2: *const OracleMatrix3,
    );
    pub fn oracle_matrix3_rotate(
        dst: *mut OracleMatrix3,
        m: *const OracleMatrix3,
        q: *const OracleQuat,
    );
    pub fn oracle_matrix3_rotate_aa(
        dst: *mut OracleMatrix3,
        m: *const OracleMatrix3,
        aa: *const OracleAxisAng,
    );
    pub fn oracle_matrix3_scale(
        dst: *mut OracleMatrix3,
        m: *const OracleMatrix3,
        v: *const OracleVec3,
    );
    pub fn oracle_matrix3_transpose(dst: *mut OracleMatrix3, m: *const OracleMatrix3);
    pub fn oracle_matrix3_inv(dst: *mut OracleMatrix3, m: *const OracleMatrix3);
    pub fn oracle_matrix3_mirror(
        dst: *mut OracleMatrix3,
        m: *const OracleMatrix3,
        p: *const OraclePlane,
    );
    pub fn oracle_matrix3_mirrorv(
        dst: *mut OracleMatrix3,
        m: *const OracleMatrix3,
        v: *const OracleVec3,
    );

    pub fn oracle_plane_from_tri(
        dst: *mut OraclePlane,
        v1: *const OracleVec3,
        v2: *const OracleVec3,
        v3: *const OracleVec3,
    );
    pub fn oracle_plane_transform(
        dst: *mut OraclePlane,
        p: *const OraclePlane,
        m: *const OracleMatrix4,
    );
    pub fn oracle_plane_transform3x4(
        dst: *mut OraclePlane,
        p: *const OraclePlane,
        m: *const OracleMatrix3,
    );
    pub fn oracle_plane_intersection_ray(
        p: *const OraclePlane,
        orig: *const OracleVec3,
        dir: *const OracleVec3,
        t: *mut f32,
    ) -> bool;
    pub fn oracle_plane_intersection_line(
        p: *const OraclePlane,
        v1: *const OracleVec3,
        v2: *const OracleVec3,
        t: *mut f32,
    ) -> bool;
    pub fn oracle_plane_tri_inside(
        p: *const OraclePlane,
        v1: *const OracleVec3,
        v2: *const OracleVec3,
        v3: *const OracleVec3,
        precision: f32,
    ) -> bool;
    pub fn oracle_plane_line_inside(
        p: *const OraclePlane,
        v1: *const OracleVec3,
        v2: *const OracleVec3,
        precision: f32,
    ) -> bool;
}

unsafe extern "C" {
    pub fn oracle_vec4_from_vec3(dst: *mut OracleVec4, v: *const OracleVec3);
    pub fn oracle_vec4_transform(
        dst: *mut OracleVec4,
        v: *const OracleVec4,
        m: *const OracleMatrix4,
    );

    pub fn oracle_vec3_size() -> usize;
    pub fn oracle_vec3_align() -> usize;
    pub fn oracle_vec4_size() -> usize;
    pub fn oracle_vec4_align() -> usize;
    pub fn oracle_vec4_offset_x() -> usize;
    pub fn oracle_vec4_offset_y() -> usize;
    pub fn oracle_vec4_offset_z() -> usize;
    pub fn oracle_vec4_offset_w() -> usize;
    pub fn oracle_vec4_offset_ptr() -> usize;
    pub fn oracle_vec4_offset_m() -> usize;
    pub fn oracle_matrix4_size() -> usize;
    pub fn oracle_matrix4_align() -> usize;
    pub fn oracle_matrix4_offset_x() -> usize;
    pub fn oracle_matrix4_offset_y() -> usize;
    pub fn oracle_matrix4_offset_z() -> usize;
    pub fn oracle_matrix4_offset_t() -> usize;

    pub fn oracle_matrix4_from_matrix3(dst: *mut OracleMatrix4, m: *const OracleMatrix3);
    pub fn oracle_matrix4_from_quat(dst: *mut OracleMatrix4, q: *const OracleQuat);
    pub fn oracle_matrix4_from_axisang(dst: *mut OracleMatrix4, aa: *const OracleAxisAng);
    pub fn oracle_matrix4_mul(
        dst: *mut OracleMatrix4,
        m1: *const OracleMatrix4,
        m2: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_determinant(m: *const OracleMatrix4) -> f32;
    pub fn oracle_matrix4_translate3v(
        dst: *mut OracleMatrix4,
        m: *const OracleMatrix4,
        v: *const OracleVec3,
    );
    pub fn oracle_matrix4_translate4v(
        dst: *mut OracleMatrix4,
        m: *const OracleMatrix4,
        v: *const OracleVec4,
    );
    pub fn oracle_matrix4_rotate(
        dst: *mut OracleMatrix4,
        m: *const OracleMatrix4,
        q: *const OracleQuat,
    );
    pub fn oracle_matrix4_rotate_aa(
        dst: *mut OracleMatrix4,
        m: *const OracleMatrix4,
        aa: *const OracleAxisAng,
    );
    pub fn oracle_matrix4_scale(
        dst: *mut OracleMatrix4,
        m: *const OracleMatrix4,
        v: *const OracleVec3,
    );
    pub fn oracle_matrix4_translate3v_i(
        dst: *mut OracleMatrix4,
        v: *const OracleVec3,
        m: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_translate4v_i(
        dst: *mut OracleMatrix4,
        v: *const OracleVec4,
        m: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_rotate_i(
        dst: *mut OracleMatrix4,
        q: *const OracleQuat,
        m: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_rotate_aa_i(
        dst: *mut OracleMatrix4,
        aa: *const OracleAxisAng,
        m: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_scale_i(
        dst: *mut OracleMatrix4,
        v: *const OracleVec3,
        m: *const OracleMatrix4,
    );
    pub fn oracle_matrix4_inv(dst: *mut OracleMatrix4, m: *const OracleMatrix4) -> bool;
    pub fn oracle_matrix4_transpose(dst: *mut OracleMatrix4, m: *const OracleMatrix4);
}

/// Independent declaration of `struct bounds`: two `vec3`s.
#[repr(C)]
#[derive(Debug, Clone, Copy, Default, PartialEq)]
pub struct OracleBounds {
    pub min: OracleVec3,
    pub max: OracleVec3,
}

unsafe extern "C" {
    pub fn oracle_bounds_size() -> usize;
    pub fn oracle_bounds_align() -> usize;
    pub fn oracle_bounds_offset_min() -> usize;
    pub fn oracle_bounds_offset_max() -> usize;

    pub fn oracle_bounds_move(dst: *mut OracleBounds, b: *const OracleBounds, v: *const OracleVec3);
    pub fn oracle_bounds_scale(
        dst: *mut OracleBounds,
        b: *const OracleBounds,
        v: *const OracleVec3,
    );
    pub fn oracle_bounds_merge(
        dst: *mut OracleBounds,
        b1: *const OracleBounds,
        b2: *const OracleBounds,
    );
    pub fn oracle_bounds_merge_point(
        dst: *mut OracleBounds,
        b: *const OracleBounds,
        v: *const OracleVec3,
    );
    pub fn oracle_bounds_get_point(dst: *mut OracleVec3, b: *const OracleBounds, i: u32);
    pub fn oracle_bounds_get_center(dst: *mut OracleVec3, b: *const OracleBounds);
    pub fn oracle_bounds_transform(
        dst: *mut OracleBounds,
        b: *const OracleBounds,
        m: *const OracleMatrix4,
    );
    pub fn oracle_bounds_transform3x4(
        dst: *mut OracleBounds,
        b: *const OracleBounds,
        m: *const OracleMatrix3,
    );
    pub fn oracle_bounds_intersection_ray(
        b: *const OracleBounds,
        orig: *const OracleVec3,
        dir: *const OracleVec3,
        t: *mut f32,
    ) -> bool;
    pub fn oracle_bounds_intersection_line(
        b: *const OracleBounds,
        p1: *const OracleVec3,
        p2: *const OracleVec3,
        t: *mut f32,
    ) -> bool;
    /// Declared `u8` because the C body returns 1, 2, or 3 from a `bool`
    /// function. The byte the compiler actually stores is what parity reads.
    pub fn oracle_bounds_plane_test(b: *const OracleBounds, p: *const OraclePlane) -> u8;
    pub fn oracle_bounds_under_plane(b: *const OracleBounds, p: *const OraclePlane) -> bool;
    pub fn oracle_bounds_intersects(
        b: *const OracleBounds,
        test: *const OracleBounds,
        epsilon: f32,
    ) -> bool;
    pub fn oracle_bounds_intersects_obb(
        b: *const OracleBounds,
        test: *const OracleBounds,
        m: *const OracleMatrix4,
        epsilon: f32,
    ) -> bool;
    pub fn oracle_bounds_intersects_obb3x4(
        b: *const OracleBounds,
        test: *const OracleBounds,
        m: *const OracleMatrix3,
        epsilon: f32,
    ) -> bool;
    pub fn oracle_bounds_min_dist(b: *const OracleBounds, p: *const OraclePlane) -> f32;

    pub fn oracle_polar_to_cart(dst: *mut OracleVec3, v: *const OracleVec3);
    pub fn oracle_cart_to_polar(dst: *mut OracleVec3, v: *const OracleVec3);
    pub fn oracle_norm_to_polar(dst: *mut OracleVec2, norm: *const OracleVec3);
    pub fn oracle_polar_to_norm(dst: *mut OracleVec3, polar: *const OracleVec2);
    pub fn oracle_calc_torquef(val1: f32, val2: f32, torque: f32, min_adjust: f32, t: f32) -> f32;
    pub fn oracle_calc_torque(
        dst: *mut OracleVec3,
        v1: *const OracleVec3,
        v2: *const OracleVec3,
        torque: f32,
        min_adjust: f32,
        t: f32,
    );
    pub fn oracle_rand_float(positive_only: c_int) -> f32;
}
