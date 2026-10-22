//! Tier 1 mirror of `test/cmocka/test_scene_group_transform.c`.
//!
//! The scene still reads the item. It still converts absolute and relative
//! coordinates, still inverts the group's draw transform when an item joins
//! a group, and still calls `update_item_transform`. The test passes the
//! matrix and the rotation delta in.
//!
//! Cases marked characterized, not endorsed, are what the C does today,
//! including the odd parts.

use obs_graphics::matrix4::Matrix4;
use obs_graphics::vec2::Vec2;
use obs_graphics::vec4::Vec4;
use obs_scene::group_transform::{GroupTransform, group_transform, transform_val};

fn expect_vec2(got: Vec2, x: f32, y: f32, what: &str) {
    assert!(
        got.x == x && got.y == y,
        "{what}: ({}, {}), expected ({x}, {y})",
        got.x,
        got.y
    );
}

fn expect_neg_zero(got: f32, what: &str) {
    assert!(
        got == 0.0 && got.is_sign_negative(),
        "{what}: {got} (signbit {}), expected -0",
        got.is_sign_negative()
    );
}

fn identity() -> Matrix4 {
    Matrix4::new(
        Vec4::new(1.0, 0.0, 0.0, 0.0),
        Vec4::new(0.0, 1.0, 0.0, 0.0),
        Vec4::new(0.0, 0.0, 1.0, 0.0),
        Vec4::new(0.0, 0.0, 0.0, 1.0),
    )
}

/// Rows. x is (0, -1), y is (1, 0): (1, 0) lands on (0, -1) before translation.
fn rotate_90(tx: f32, ty: f32) -> Matrix4 {
    Matrix4::new(
        Vec4::new(0.0, -1.0, 0.0, 0.0),
        Vec4::new(1.0, 0.0, 0.0, 0.0),
        Vec4::new(0.0, 0.0, 1.0, 0.0),
        Vec4::new(tx, ty, 0.0, 1.0),
    )
}

fn apply(transform: &Matrix4, pos: Vec2, scale: Vec2, rot: f32, rot_delta: f32) -> GroupTransform {
    group_transform(transform, pos, scale, rot, rot_delta)
}

#[test]
fn identity_keeps_pos_and_scale_and_adds_rot() {
    let m = identity();
    let out = apply(&m, Vec2::new(4.0, 5.0), Vec2::new(2.0, 3.0), 10.0, 90.0);
    expect_vec2(out.pos, 4.0, 5.0, "identity position");
    expect_vec2(out.scale, 2.0, 3.0, "identity scale");
    assert_eq!(out.rot, 100.0);
}

/// Translation moves the point. It is cleared before the scale multiply, so
/// the scale stays. The caller's matrix keeps its translation.
#[test]
fn translation_moves_pos_only() {
    let mut m = identity();
    m.t = Vec4::new(10.0, 20.0, 0.0, 1.0);
    let out = apply(&m, Vec2::new(1.0, 2.0), Vec2::new(3.0, 4.0), 1.0, 0.0);
    expect_vec2(out.pos, 11.0, 22.0, "translated position");
    expect_vec2(out.scale, 3.0, 4.0, "translation does not scale");
    assert_eq!(out.rot, 1.0);
    assert_eq!(m.t.x, 10.0);
    assert_eq!(m.t.y, 20.0);
    assert_eq!(m.t.w, 1.0);
}

#[test]
fn axis_scale_multiplies_pos_and_scale() {
    let mut m = identity();
    m.x = Vec4::new(2.0, 0.0, 0.0, 0.0);
    m.y = Vec4::new(0.0, 3.0, 0.0, 0.0);
    let out = apply(&m, Vec2::new(1.0, 1.0), Vec2::new(4.0, 5.0), 0.0, 0.0);
    expect_vec2(out.pos, 2.0, 3.0, "scaled position");
    expect_vec2(out.scale, 8.0, 15.0, "scaled axes");
}

/// The matrix rotation moves the point. Scale is the column length, so a
/// pure rotation leaves it alone. Rotation of the child is only rot_delta.
#[test]
fn rotation_moves_pos_and_keeps_scale() {
    let m = rotate_90(0.0, 0.0);
    let out = apply(&m, Vec2::new(1.0, 0.0), Vec2::new(2.0, 3.0), 10.0, 90.0);
    expect_vec2(out.pos, 0.0, -1.0, "rotated position");
    expect_vec2(out.scale, 2.0, 3.0, "rotation does not scale");
    assert_eq!(out.rot, 100.0);
}

#[test]
fn negative_scale_keeps_its_sign() {
    let mut m = identity();
    let out = apply(&m, Vec2::new(1.0, 1.0), Vec2::new(-2.0, 3.0), 0.0, 0.0);
    expect_vec2(out.scale, -2.0, 3.0, "negative axis stays negative");

    m.x = Vec4::new(2.0, 0.0, 0.0, 0.0);
    let out = apply(&m, Vec2::new(1.0, 1.0), Vec2::new(-2.0, 3.0), 0.0, -90.0);
    expect_vec2(out.scale, -4.0, 3.0, "negative axis through a 2x scale");
    assert_eq!(out.rot, -90.0);
}

/// A scale that is not greater than zero takes the minus sign. The length of
/// a zero column is positive zero, so the product is negative zero. The same
/// for an input of -0. Characterized, not endorsed.
#[test]
fn zero_scale_is_negative_zero() {
    let m = identity();
    let out = apply(&m, Vec2::new(1.0, 2.0), Vec2::new(0.0, 0.0), 7.0, 0.0);
    expect_vec2(out.pos, 1.0, 2.0, "zero scale still translates nothing");
    expect_neg_zero(out.scale.x, "zero x");
    expect_neg_zero(out.scale.y, "zero y");
    assert_eq!(out.rot, 7.0);

    let out = apply(&m, Vec2::new(1.0, 2.0), Vec2::new(-0.0, 4.0), 7.0, 0.0);
    expect_neg_zero(out.scale.x, "negative zero stays negative zero");
    assert_eq!(out.scale.y, 4.0);
}

/// `resize_group` measures a point through the group's draw transform.
/// (1, 0) through the 90 degree matrix plus (5, 7) is (5, 6).
#[test]
fn transform_val_includes_translation() {
    let m = rotate_90(5.0, 7.0);
    let point = transform_val(Vec2::new(1.0, 0.0), &m);
    expect_vec2(point, 5.0, 6.0, "point through rotation and translation");
    assert_eq!(m.t.x, 5.0);
    assert_eq!(m.t.y, 7.0);
}
