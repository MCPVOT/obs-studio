//! Tier 3: the shims against the original `obs-scene-coords.c.h`.
//!
//! No intentional differences. A null pointer is not called. The half-pixel
//! nudge and the missing round trip are characterized, not endorsed, and
//! both sides keep them. `test_scene_coords` compiles the C into itself, so
//! this is what compares the swap.

use obs_c_oracle::scene_coords::{
    oracle_obs_scene_coords_keep_scale, oracle_obs_scene_coords_nudge,
    oracle_obs_scene_item_canvas_scale, oracle_obs_scene_item_relative_scale,
    oracle_obs_scene_nudge_abs, oracle_obs_scene_pos_from_absolute,
    oracle_obs_scene_pos_to_absolute, oracle_obs_scene_size_from_absolute,
    oracle_obs_scene_size_to_absolute,
};
use obs_c_oracle::vec2::OracleVec2;
use obs_graphics::ffi::vec2::vec2;
use obs_scene::ffi::scene_coords::{
    obs_scene_coords_keep_scale, obs_scene_coords_nudge, obs_scene_item_canvas_scale,
    obs_scene_item_relative_scale, obs_scene_nudge_abs, obs_scene_pos_from_absolute,
    obs_scene_pos_to_absolute, obs_scene_size_from_absolute, obs_scene_size_to_absolute,
};
use proptest::prelude::*;

/// Bit-exact equality, except that any NaN equals any NaN.
fn same(a: f32, b: f32) -> bool {
    a.to_bits() == b.to_bits() || (a.is_nan() && b.is_nan())
}

fn pair(x: f32, y: f32) -> (vec2, OracleVec2) {
    (vec2 { x, y }, OracleVec2 { x, y })
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

fn check_nudge_abs(x: f32, y: f32) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: all four are live stack values. The shim and the oracle
    // write dst and leave v alone.
    unsafe {
        obs_scene_nudge_abs(&raw mut rust_dst, &raw const rust_v);
        oracle_obs_scene_nudge_abs(&raw mut c_dst, &raw const c_v);
    }
    assert!(same(rust_v.x, x) && same(rust_v.y, y), "rust wrote v");
    assert_vec("nudge_abs", rust_dst, c_dst);
}

fn check_pos_to(x: f32, y: f32, canvas_x: f32, canvas_y: f32, nudge: bool) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and v do not alias.
    unsafe {
        obs_scene_pos_to_absolute(
            &raw mut rust_dst,
            &raw const rust_v,
            canvas_x,
            canvas_y,
            nudge,
        );
        oracle_obs_scene_pos_to_absolute(&raw mut c_dst, &raw const c_v, canvas_x, canvas_y, nudge);
    }
    assert!(same(rust_v.x, x) && same(rust_v.y, y), "rust wrote v");
    assert_vec("pos_to", rust_dst, c_dst);
}

fn check_pos_from(x: f32, y: f32, canvas_x: f32, canvas_y: f32) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and v do not alias.
    unsafe {
        obs_scene_pos_from_absolute(&raw mut rust_dst, &raw const rust_v, canvas_x, canvas_y);
        oracle_obs_scene_pos_from_absolute(&raw mut c_dst, &raw const c_v, canvas_x, canvas_y);
    }
    assert_vec("pos_from", rust_dst, c_dst);
}

fn check_size_to(x: f32, y: f32, canvas_y: f32, nudge: bool) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and v do not alias.
    unsafe {
        obs_scene_size_to_absolute(&raw mut rust_dst, &raw const rust_v, canvas_y, nudge);
        oracle_obs_scene_size_to_absolute(&raw mut c_dst, &raw const c_v, canvas_y, nudge);
    }
    assert_vec("size_to", rust_dst, c_dst);
}

fn check_size_from(x: f32, y: f32, canvas_y: f32) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and v do not alias.
    unsafe {
        obs_scene_size_from_absolute(&raw mut rust_dst, &raw const rust_v, canvas_y);
        oracle_obs_scene_size_from_absolute(&raw mut c_dst, &raw const c_v, canvas_y);
    }
    assert_vec("size_from", rust_dst, c_dst);
}

fn check_canvas_scale(x: f32, y: f32, canvas_y: f32, scale_ref_y: f32) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and scale do not alias.
    unsafe {
        obs_scene_item_canvas_scale(&raw mut rust_dst, &raw const rust_v, canvas_y, scale_ref_y);
        oracle_obs_scene_item_canvas_scale(&raw mut c_dst, &raw const c_v, canvas_y, scale_ref_y);
    }
    assert_vec("canvas_scale", rust_dst, c_dst);
}

fn check_relative_scale(x: f32, y: f32, canvas_y: f32, scale_ref_y: f32) {
    let (rust_v, c_v) = pair(x, y);
    let (mut rust_dst, mut c_dst) = pair(0.0, 0.0);
    // SAFETY: live stack values. dst and v do not alias.
    unsafe {
        obs_scene_item_relative_scale(&raw mut rust_dst, &raw const rust_v, canvas_y, scale_ref_y);
        oracle_obs_scene_item_relative_scale(&raw mut c_dst, &raw const c_v, canvas_y, scale_ref_y);
    }
    assert_vec("relative_scale", rust_dst, c_dst);
}

fn check_all(x: f32, y: f32, canvas_x: f32, canvas_y: f32, scale_ref_y: f32, nudge: bool) {
    check_nudge_abs(x, y);
    check_pos_to(x, y, canvas_x, canvas_y, nudge);
    check_pos_from(x, y, canvas_x, canvas_y);
    check_size_to(x, y, canvas_y, nudge);
    check_size_from(x, y, canvas_y);
    check_canvas_scale(x, y, canvas_y, scale_ref_y);
    check_relative_scale(x, y, canvas_y, scale_ref_y);
}

/// The cmocka cases, through the shim. Includes the half-pixel ties and the
/// value that does not round-trip once nudged.
#[test]
fn cmocka_cases_match_oracle() {
    for (group, parent) in [(false, false), (true, false), (false, true), (true, true)] {
        assert_eq!(
            obs_scene_coords_nudge(group, parent),
            unsafe { oracle_obs_scene_coords_nudge(group, parent) },
            "nudge {group} {parent}"
        );
    }
    for (group, scene) in [(false, false), (true, false), (false, true), (true, true)] {
        assert_eq!(
            obs_scene_coords_keep_scale(group, scene),
            unsafe { oracle_obs_scene_coords_keep_scale(group, scene) },
            "keep {group} {scene}"
        );
    }

    check_all(0.0, 1.0, 1920.0, 1080.0, 1080.0, true);
    check_all(0.5, 1.5, 1920.0, 1080.0, 1080.0, true);
    check_all(0.25, 0.75, 0.0, 6.0, 1080.0, true);
    check_all(0.25, 0.0, 0.0, 6.0, 1080.0, false);
    check_all(-0.25, -0.75, 1920.0, 1080.0, 1080.0, true);
    check_all(-0.5, -1.0, 1920.0, 1080.0, 540.0, true);
    check_all(0.0, 0.0, 1920.0, 1080.0, 1080.0, true);
    check_all(1.0, 1.0, 1920.0, 1080.0, 1080.0, true);
    check_all(-1.0, -1.0, 1920.0, 1080.0, 1080.0, true);
    check_all(0.75, 3.0, 0.0, 6.0, 6.0, false);
    check_all(960.0, 540.0, 1920.0, 1080.0, 1080.0, false);
    check_all(1.0, 3.0, 0.0, 6.0, 6.0, false);
    check_all(2.0, 2.0, 1920.0, 1080.0, 1080.0, true);
    check_all(1080.0, 1080.0, 1920.0, 1080.0, 1080.0, false);
    check_all(-0.1875, 0.5, 16.0, 8.0, 8.0, true);
    check_all(-0.1875, 0.5, 16.0, 8.0, 8.0, false);
    check_all(-0.75, 2.0, 16.0, 8.0, 8.0, false);
    check_all(1.5, -2.0, 3840.0, 2160.0, 1080.0, false);
    check_all(3.0, -4.0, 3840.0, 2160.0, 1080.0, false);
    check_all(2.0, 4.0, 960.0, 540.0, 1080.0, false);
    check_all(1.0, -1.0, 1920.0, 1080.0, 1080.0, false);
}

/// dst and the input are the same pointer, as the cmocka test does.
#[test]
fn aliased_pointer_matches_oracle() {
    let mut rust = vec2 { x: 0.25, y: 0.75 };
    let mut c = OracleVec2 { x: 0.25, y: 0.75 };
    // SAFETY: each pointer is a live stack value, passed as both arguments.
    unsafe {
        obs_scene_nudge_abs(&raw mut rust, &raw const rust);
        oracle_obs_scene_nudge_abs(&raw mut c, &raw const c);
    }
    assert_vec("aliased nudge", rust, c);

    let mut rust = vec2 { x: 0.25, y: 0.0 };
    let mut c = OracleVec2 { x: 0.25, y: 0.0 };
    // SAFETY: same as above.
    unsafe {
        obs_scene_pos_to_absolute(&raw mut rust, &raw const rust, 0.0, 6.0, true);
        oracle_obs_scene_pos_to_absolute(&raw mut c, &raw const c, 0.0, 6.0, true);
    }
    assert_vec("aliased pos_to", rust, c);
}

/// Any bit pattern, so NaNs, infinities, zeros and subnormals all appear.
fn any_f32() -> impl Strategy<Value = f32> {
    any::<u32>().prop_map(f32::from_bits)
}

proptest! {
    #[test]
    fn random_inputs_match_oracle(
        x in any_f32(),
        y in any_f32(),
        canvas_x in any_f32(),
        canvas_y in any_f32(),
        scale_ref_y in any_f32(),
        nudge in any::<bool>(),
        is_group in any::<bool>(),
        parent_is_group in any::<bool>(),
        is_scene in any::<bool>(),
    ) {
        assert_eq!(
            obs_scene_coords_nudge(is_group, parent_is_group),
            unsafe { oracle_obs_scene_coords_nudge(is_group, parent_is_group) }
        );
        assert_eq!(
            obs_scene_coords_keep_scale(is_group, is_scene),
            unsafe { oracle_obs_scene_coords_keep_scale(is_group, is_scene) }
        );
        check_all(x, y, canvas_x, canvas_y, scale_ref_y, nudge);
    }
}
