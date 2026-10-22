//! Tier 3: the shims against the original `obs-scene-group-transform.c.h`.
//!
//! No intentional differences. A null pointer is not called. A zero scale
//! becomes negative zero, and a rotation in the matrix does not change the
//! scale. Both are characterized, not endorsed, and both sides keep them.
//! The C writes `x_base` and never reads it; the Rust core skips that
//! store. `test_scene_group_transform` compiles the C into itself, so this
//! is what compares the swap.
//!
//! `struct vec2` and `struct matrix4` are the graphics layouts. This checks
//! those sizes against the oracle before it compares values.

use std::mem::{align_of, size_of};

use obs_c_oracle::graphics_math::{
    OracleMatrix4, OracleVec4, oracle_matrix4_align, oracle_matrix4_offset_t, oracle_matrix4_size,
};
use obs_c_oracle::scene_group_transform::{
    oracle_obs_scene_group_transform, oracle_obs_scene_transform_val,
};
use obs_c_oracle::vec2::OracleVec2;
use obs_graphics::ffi::matrix4::matrix4;
use obs_graphics::ffi::vec2::vec2;
use obs_graphics::ffi::vec4::vec4;
use obs_scene::ffi::scene_group_transform::{obs_scene_group_transform, obs_scene_transform_val};
use proptest::prelude::*;

/// Bit-exact equality, except that any NaN equals any NaN. `-0` and `+0`
/// stay distinct.
fn same(a: f32, b: f32) -> bool {
    a.to_bits() == b.to_bits() || (a.is_nan() && b.is_nan())
}

fn pair(x: f32, y: f32) -> (vec2, OracleVec2) {
    (vec2 { x, y }, OracleVec2 { x, y })
}

fn row(x: f32, y: f32, z: f32, w: f32) -> (vec4, OracleVec4) {
    (vec4 { x, y, z, w }, OracleVec4 { x, y, z, w })
}

fn matrix(rows: [[f32; 4]; 4]) -> (matrix4, OracleMatrix4) {
    let (x, ox) = row(rows[0][0], rows[0][1], rows[0][2], rows[0][3]);
    let (y, oy) = row(rows[1][0], rows[1][1], rows[1][2], rows[1][3]);
    let (z, oz) = row(rows[2][0], rows[2][1], rows[2][2], rows[2][3]);
    let (t, ot) = row(rows[3][0], rows[3][1], rows[3][2], rows[3][3]);
    (
        matrix4 { x, y, z, t },
        OracleMatrix4 {
            x: ox,
            y: oy,
            z: oz,
            t: ot,
        },
    )
}

fn assert_vec(what: &str, rust: vec2, oracle: OracleVec2) {
    assert!(
        same(rust.x, oracle.x) && same(rust.y, oracle.y),
        "{what}: rust ({}, {}) oracle ({}, {})",
        rust.x,
        rust.y,
        oracle.x,
        oracle.y
    );
}

fn assert_row(what: &str, got: vec4, expected: vec4) {
    assert!(
        same(got.x, expected.x)
            && same(got.y, expected.y)
            && same(got.z, expected.z)
            && same(got.w, expected.w),
        "{what}: ({}, {}, {}, {}), expected ({}, {}, {}, {})",
        got.x,
        got.y,
        got.z,
        got.w,
        expected.x,
        expected.y,
        expected.z,
        expected.w
    );
}

fn assert_unchanged(what: &str, got: matrix4, expected: matrix4) {
    assert_row(&format!("{what} x"), got.x, expected.x);
    assert_row(&format!("{what} y"), got.y, expected.y);
    assert_row(&format!("{what} z"), got.z, expected.z);
    assert_row(&format!("{what} t"), got.t, expected.t);
}

fn check_transform_val(px: f32, py: f32, rows: [[f32; 4]; 4]) {
    let (mut rust_dst, mut c_dst) = pair(px, py);
    let (rust_m, c_m) = matrix(rows);
    let rust_before = rust_m;
    // SAFETY: live stack values. dst and the matrix do not alias.
    unsafe {
        obs_scene_transform_val(&raw mut rust_dst, &raw const rust_m);
        oracle_obs_scene_transform_val(&raw mut c_dst, &raw const c_m);
    }
    assert_vec("transform_val", rust_dst, c_dst);
    assert_unchanged("transform_val matrix", rust_m, rust_before);
}

fn check_group(px: f32, py: f32, sx: f32, sy: f32, rot: f32, rot_delta: f32, rows: [[f32; 4]; 4]) {
    let (mut rust_pos, mut c_pos) = pair(px, py);
    let (mut rust_scale, mut c_scale) = pair(sx, sy);
    let mut rust_rot = rot;
    let mut c_rot = rot;
    let (rust_m, c_m) = matrix(rows);
    let rust_before = rust_m;
    // SAFETY: live stack values. pos, scale, rot, and the matrix are distinct.
    unsafe {
        obs_scene_group_transform(
            &raw const rust_m,
            &raw mut rust_pos,
            &raw mut rust_scale,
            &raw mut rust_rot,
            rot_delta,
        );
        oracle_obs_scene_group_transform(
            &raw const c_m,
            &raw mut c_pos,
            &raw mut c_scale,
            &raw mut c_rot,
            rot_delta,
        );
    }
    assert_vec("pos", rust_pos, c_pos);
    assert_vec("scale", rust_scale, c_scale);
    assert!(same(rust_rot, c_rot), "rot: rust {rust_rot} oracle {c_rot}");
    assert_unchanged("group matrix", rust_m, rust_before);
}

fn identity() -> [[f32; 4]; 4] {
    [
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 1.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ]
}

/// The cmocka cases, through the shim.
#[test]
fn cmocka_cases_match_oracle() {
    check_group(4.0, 5.0, 2.0, 3.0, 10.0, 90.0, identity());

    let mut translated = identity();
    translated[3] = [10.0, 20.0, 0.0, 1.0];
    check_group(1.0, 2.0, 3.0, 4.0, 1.0, 0.0, translated);

    let mut scaled = identity();
    scaled[0][0] = 2.0;
    scaled[1][1] = 3.0;
    check_group(1.0, 1.0, 4.0, 5.0, 0.0, 0.0, scaled);

    let rotated = [
        [0.0, -1.0, 0.0, 0.0],
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [0.0, 0.0, 0.0, 1.0],
    ];
    check_group(1.0, 0.0, 2.0, 3.0, 10.0, 90.0, rotated);

    check_group(1.0, 1.0, -2.0, 3.0, 0.0, 0.0, identity());
    let mut doubled_x = identity();
    doubled_x[0][0] = 2.0;
    check_group(1.0, 1.0, -2.0, 3.0, 0.0, -90.0, doubled_x);

    check_group(1.0, 2.0, 0.0, 0.0, 7.0, 0.0, identity());
    check_group(1.0, 2.0, -0.0, 4.0, 7.0, 0.0, identity());

    let rotated_moved = [
        [0.0, -1.0, 0.0, 0.0],
        [1.0, 0.0, 0.0, 0.0],
        [0.0, 0.0, 1.0, 0.0],
        [5.0, 7.0, 0.0, 1.0],
    ];
    check_transform_val(1.0, 0.0, rotated_moved);
}

#[test]
fn layouts_match_the_c_structs() {
    // SAFETY: the oracle functions take no pointers.
    let (size, align, offset_t) = unsafe {
        (
            oracle_matrix4_size(),
            oracle_matrix4_align(),
            oracle_matrix4_offset_t(),
        )
    };
    assert_eq!(size_of::<matrix4>(), size);
    assert_eq!(align_of::<matrix4>(), align);
    assert_eq!(std::mem::offset_of!(matrix4, t), offset_t);
    assert_eq!(size_of::<vec2>(), 8);
    assert_eq!(align_of::<vec2>(), 4);
}

/// Any bit pattern, so NaNs, infinities, zeros and subnormals all appear.
fn any_f32() -> impl Strategy<Value = f32> {
    any::<u32>().prop_map(f32::from_bits)
}

proptest! {
    #[test]
    fn random_inputs_match_oracle(
        px in any_f32(),
        py in any_f32(),
        sx in any_f32(),
        sy in any_f32(),
        rot in any_f32(),
        rot_delta in any_f32(),
        m00 in any_f32(),
        m01 in any_f32(),
        m02 in any_f32(),
        m03 in any_f32(),
        m10 in any_f32(),
        m11 in any_f32(),
        m12 in any_f32(),
        m13 in any_f32(),
        m20 in any_f32(),
        m21 in any_f32(),
        m22 in any_f32(),
        m23 in any_f32(),
        m30 in any_f32(),
        m31 in any_f32(),
        m32 in any_f32(),
        m33 in any_f32(),
    ) {
        let rows = [
            [m00, m01, m02, m03],
            [m10, m11, m12, m13],
            [m20, m21, m22, m23],
            [m30, m31, m32, m33],
        ];
        check_transform_val(px, py, rows);
        check_group(px, py, sx, sy, rot, rot_delta, rows);
    }
}
