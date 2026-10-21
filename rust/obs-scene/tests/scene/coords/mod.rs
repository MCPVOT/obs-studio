//! Tier 1 mirror of `test/cmocka/test_scene_coords.c`.
//!
//! The scene still reads the canvas and still decides two things before it
//! calls the core: whether to nudge onto a half pixel, and whether a group
//! or nested scene keeps its scale. The test passes the canvas size and
//! those flags in.
//!
//! Cases marked characterized, not endorsed, are what the C does today,
//! including the odd parts.

mod parity;

use obs_graphics::vec2::Vec2;
use obs_scene::coords::{
    coords_keep_scale, coords_nudge, item_canvas_scale, item_relative_scale, nudge_abs,
    pos_from_absolute, pos_to_absolute, size_from_absolute, size_to_absolute,
};

fn expect_vec2(got: Vec2, x: f32, y: f32, what: &str) {
    assert!(
        got.x == x && got.y == y,
        "{what}: ({}, {}), expected ({x}, {y})",
        got.x,
        got.y
    );
}

/// `floor(v * 2 + 0.5) / 2`. A value already on a half stays. 0.25 and 0.75
/// move up. -0.25 moves to 0, and -0.75 moves to -0.5, so a negative tie does
/// not mirror the positive one. Characterized, not endorsed.
#[test]
fn nudge_abs_rounds_to_a_half() {
    expect_vec2(nudge_abs(Vec2::new(0.0, 1.0)), 0.0, 1.0, "on a pixel");
    expect_vec2(nudge_abs(Vec2::new(0.5, 1.5)), 0.5, 1.5, "on a half");
    expect_vec2(
        nudge_abs(Vec2::new(0.25, 0.75)),
        0.5,
        1.0,
        "positive ties go up",
    );
    expect_vec2(
        nudge_abs(Vec2::new(-0.25, -0.75)),
        0.0,
        -0.5,
        "negative ties",
    );
    expect_vec2(
        nudge_abs(Vec2::new(-0.5, -1.0)),
        -0.5,
        -1.0,
        "negative halves stay",
    );
}

#[test]
fn nudge_skips_a_group() {
    assert!(coords_nudge(false, false));
    assert!(!coords_nudge(true, false));
    assert!(!coords_nudge(false, true));
    assert!(!coords_nudge(true, true));
}

#[test]
fn keep_scale_for_a_group_or_nested_scene() {
    assert!(!coords_keep_scale(false, false));
    assert!(coords_keep_scale(true, false));
    assert!(coords_keep_scale(false, true));
    assert!(coords_keep_scale(true, true));
}

/// 1920x1080. Height is [-1, 1], so the center is (0, 0) and a corner is an
/// axis of 1. These land on whole pixels, so the nudge does not move them.
#[test]
fn pos_to_absolute_on_a_1080p_canvas() {
    expect_vec2(
        pos_to_absolute(Vec2::new(0.0, 0.0), 1920.0, 1080.0, true),
        960.0,
        540.0,
        "center",
    );
    expect_vec2(
        pos_to_absolute(Vec2::new(1.0, 1.0), 1920.0, 1080.0, true),
        1500.0,
        1080.0,
        "bottom right of the relative box",
    );
    expect_vec2(
        pos_to_absolute(Vec2::new(-1.0, -1.0), 1920.0, 1080.0, true),
        420.0,
        0.0,
        "top left of the relative box",
    );
}

/// Canvas width 0 and height 6. (0.25, 0) is x = 0.75 before the nudge, and
/// y = 3. The nudge moves 0.75 to 1 and leaves 3. Without the nudge, 0.75
/// stays. Characterized, not endorsed.
#[test]
fn pos_to_absolute_nudge_moves_a_quarter() {
    expect_vec2(
        pos_to_absolute(Vec2::new(0.25, 0.0), 0.0, 6.0, true),
        1.0,
        3.0,
        "nudged",
    );
    expect_vec2(
        pos_to_absolute(Vec2::new(0.25, 0.0), 0.0, 6.0, false),
        0.75,
        3.0,
        "group keeps 0.75",
    );
}

/// The way back does not nudge. 0.75 pixels on that same canvas is 0.25
/// again. Running the nudge first does not come back to 0.25.
#[test]
fn pos_from_absolute_does_not_nudge() {
    expect_vec2(
        pos_from_absolute(Vec2::new(0.75, 3.0), 0.0, 6.0),
        0.25,
        0.0,
        "0.75 pixels",
    );
    expect_vec2(
        pos_from_absolute(Vec2::new(960.0, 540.0), 1920.0, 1080.0),
        0.0,
        0.0,
        "1080p center",
    );
    expect_vec2(
        pos_from_absolute(Vec2::new(1.0, 3.0), 0.0, 6.0),
        1.0 / 3.0,
        0.0,
        "after a nudge",
    );
}

/// Relative size 2 is the full canvas height. A negative size is a flip, and
/// the nudge of -0.75 is -0.5.
#[test]
fn size_to_absolute_and_back() {
    expect_vec2(
        size_to_absolute(Vec2::new(2.0, 2.0), 1080.0, true),
        1080.0,
        1080.0,
        "full height",
    );
    expect_vec2(
        size_from_absolute(Vec2::new(1080.0, 1080.0), 1080.0),
        2.0,
        2.0,
        "full height back",
    );
    expect_vec2(
        size_to_absolute(Vec2::new(-0.1875, 0.5), 8.0, true),
        -0.5,
        2.0,
        "flipped quarter nudged",
    );
    expect_vec2(
        size_to_absolute(Vec2::new(-0.1875, 0.5), 8.0, false),
        -0.75,
        2.0,
        "flipped quarter kept",
    );
    expect_vec2(
        size_from_absolute(Vec2::new(-0.75, 2.0), 8.0),
        -0.1875,
        0.5,
        "size back does not nudge",
    );
}

/// Doubling the canvas height doubles the scale, including a flip. Halving
/// it halves the scale. The relative function is that in reverse.
#[test]
fn canvas_scale_follows_the_height() {
    expect_vec2(
        item_canvas_scale(Vec2::new(1.5, -2.0), 2160.0, 1080.0),
        3.0,
        -4.0,
        "canvas grew",
    );
    expect_vec2(
        item_relative_scale(Vec2::new(3.0, -4.0), 2160.0, 1080.0),
        1.5,
        -2.0,
        "back to the reference",
    );
    expect_vec2(
        item_canvas_scale(Vec2::new(2.0, 4.0), 540.0, 1080.0),
        1.0,
        2.0,
        "canvas shrank",
    );
    expect_vec2(
        item_canvas_scale(Vec2::new(1.0, -1.0), 1080.0, 1080.0),
        1.0,
        -1.0,
        "same canvas",
    );
}
