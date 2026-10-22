//! Tier 1: every assertion in test_scene_bounds_place.c, including characterized oddities.
use obs_graphics::vec2::Vec2;
use obs_scene::bounds::BoundsType;
use obs_scene::bounds_place::*;

#[allow(clippy::too_many_arguments)]
fn place(
    kind: BoundsType,
    bx: f32,
    by: f32,
    ox: f32,
    oy: f32,
    sx: f32,
    sy: f32,
    cx: u32,
    cy: u32,
    align: u32,
    crop: bool,
) -> Placement {
    bounds_place(
        kind,
        Vec2::new(bx, by),
        Placement {
            origin: Vec2::new(ox, oy),
            scale: Vec2::new(sx, sy),
            cx,
            cy,
            crop: Crop {
                left: 7,
                top: 7,
                right: 7,
                bottom: 7,
            },
        },
        align,
        crop,
    )
}
#[allow(clippy::too_many_arguments)]
fn expect_placed(
    got: Placement,
    ox: f32,
    oy: f32,
    sx: f32,
    sy: f32,
    cx: u32,
    cy: u32,
    left: i32,
    top: i32,
    right: i32,
    bottom: i32,
    what: &str,
) {
    assert_eq!(
        got,
        Placement {
            origin: Vec2::new(ox, oy),
            scale: Vec2::new(sx, sy),
            cx,
            cy,
            crop: Crop {
                left,
                top,
                right,
                bottom
            }
        },
        "{what}"
    );
}
#[test]
fn test_scene_bounds_place_add_alignment_picks_a_side() {
    for (v, a, cx, cy, want) in [
        (Vec2::new(1.0, 2.0), LEFT | TOP, 5, 5, Vec2::new(1.0, 2.0)),
        (
            Vec2::new(1.0, 2.0),
            RIGHT | BOTTOM,
            5,
            9,
            Vec2::new(6.0, 11.0),
        ),
        (Vec2::new(0.0, 0.0), CENTER, 5, -5, Vec2::new(2.0, -2.0)),
        (Vec2::new(0.0, 0.0), LEFT | RIGHT, 8, 5, Vec2::new(8.0, 2.0)),
        (Vec2::new(0.0, 0.0), TOP | BOTTOM, 5, 8, Vec2::new(2.0, 8.0)),
    ] {
        assert_eq!(add_alignment(v, a, cx, cy), want);
    }
}

#[test]
fn test_scene_bounds_place_none_centers_the_leftover() {
    let p = place(
        BoundsType::None,
        100.0,
        100.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        false,
    );
    expect_placed(p, 50.0, 0.0, 1.0, 1.0, 100, 100, 7, 7, 7, 7, "none center");
}

#[test]
fn test_scene_bounds_place_none_ignores_the_crop_flag_when_both_axes_overflow() {
    let p = place(
        BoundsType::None,
        50.0,
        40.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        true,
    );
    expect_placed(
        p,
        75.0,
        30.0,
        1.0,
        1.0,
        50,
        40,
        7,
        7,
        7,
        7,
        "none crop flag",
    );
}

#[test]
fn test_scene_bounds_place_stretch_matches_the_box() {
    let p = place(
        BoundsType::Stretch,
        100.0,
        100.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        false,
    );
    expect_placed(p, 0.0, 0.0, 0.5, 1.0, 100, 100, 7, 7, 7, 7, "stretch");
}

#[test]
fn test_scene_bounds_place_scale_inner_top_left_and_center() {
    let top_left = place(
        BoundsType::ScaleInner,
        100.0,
        80.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        LEFT | TOP,
        false,
    );
    expect_placed(
        top_left,
        0.0,
        0.0,
        0.5,
        0.5,
        100,
        80,
        7,
        7,
        7,
        7,
        "inner top left",
    );

    let center = place(
        BoundsType::ScaleInner,
        100.0,
        80.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        false,
    );
    expect_placed(
        center,
        0.0,
        -15.0,
        0.5,
        0.5,
        100,
        80,
        7,
        7,
        7,
        7,
        "inner center",
    );
}

#[test]
fn test_scene_bounds_place_scale_outer_crop_drops_that_axis_of_alignment() {
    let scale = 0.8;

    let p = place(
        BoundsType::ScaleOuter,
        50.0,
        80.0,
        9.0,
        3.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        true,
    );
    expect_placed(
        p,
        0.0,
        3.0,
        scale,
        scale,
        50,
        80,
        69,
        7,
        69,
        7,
        "outer crop",
    );
}

#[test]
fn test_scene_bounds_place_scale_outer_without_crop_keeps_alignment() {
    let scale = 0.8;

    let p = place(
        BoundsType::ScaleOuter,
        50.0,
        80.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        false,
    );
    expect_placed(
        p,
        55.0,
        0.0,
        scale,
        scale,
        50,
        80,
        7,
        7,
        7,
        7,
        "outer no crop",
    );
}

#[test]
fn test_scene_bounds_place_flipped_item_stays_in_the_box() {
    let p = place(
        BoundsType::None,
        10.0,
        10.0,
        0.0,
        0.0,
        -1.0,
        1.0,
        10,
        10,
        CENTER,
        false,
    );
    expect_placed(p, -10.0, 0.0, -1.0, 1.0, 10, 10, 7, 7, 7, 7, "flip none");
}

#[test]
fn test_scene_bounds_place_fractional_box_truncates_toward_zero() {
    let p = place(
        BoundsType::None,
        10.9,
        4.0,
        0.0,
        0.0,
        1.0,
        1.0,
        4,
        4,
        CENTER,
        false,
    );
    expect_placed(p, -3.0, 0.0, 1.0, 1.0, 10, 4, 7, 7, 7, 7, "fractional");
}

#[test]
fn test_scene_bounds_place_round_half_away_from_zero() {
    let p = place(
        BoundsType::ScaleOuter,
        1.5,
        10.0,
        0.0,
        0.0,
        1.0,
        1.0,
        2,
        0,
        LEFT | TOP,
        true,
    );
    expect_placed(p, 0.0, 0.0, 1.0, 1.0, 1, 10, 0, 7, 1, 7, "round half");
}

#[test]
fn test_scene_bounds_place_crop_threshold_is_strict() {
    let above = place(
        BoundsType::ScaleToWidth,
        16.0,
        15.9375,
        3.0,
        4.0,
        1.0,
        1.0,
        16,
        16,
        LEFT | TOP,
        true,
    );
    expect_placed(above, 3.0, 4.0, 1.0, 1.0, 16, 15, 7, 7, 7, 7, "above -0.1");

    let below = place(
        BoundsType::ScaleToWidth,
        16.0,
        15.75,
        3.0,
        4.0,
        1.0,
        1.0,
        16,
        16,
        LEFT | TOP,
        true,
    );
    expect_placed(below, 3.0, 0.0, 1.0, 1.0, 16, 15, 7, 0, 7, 0, "below -0.1");
}

#[test]
fn test_scene_bounds_place_flipped_crop_center_left_and_right() {
    let center = place(
        BoundsType::ScaleOuter,
        10.0,
        10.0,
        0.0,
        0.0,
        -2.0,
        1.0,
        20,
        0,
        CENTER,
        true,
    );
    expect_placed(
        center,
        -10.0,
        -5.0,
        -2.0,
        1.0,
        10,
        10,
        8,
        7,
        8,
        7,
        "flipped center",
    );

    let left = place(
        BoundsType::ScaleOuter,
        10.0,
        10.0,
        0.0,
        0.0,
        -1.0,
        1.0,
        20,
        0,
        LEFT | TOP,
        true,
    );
    expect_placed(
        left,
        -10.0,
        0.0,
        -1.0,
        1.0,
        10,
        10,
        10,
        7,
        0,
        7,
        "flipped left",
    );

    let right = place(
        BoundsType::ScaleOuter,
        10.0,
        10.0,
        0.0,
        0.0,
        -1.0,
        1.0,
        20,
        0,
        RIGHT | TOP,
        true,
    );
    expect_placed(
        right,
        -10.0,
        0.0,
        -1.0,
        1.0,
        10,
        10,
        0,
        7,
        10,
        7,
        "flipped right",
    );
}

#[test]
fn test_scene_bounds_place_empty_source_keeps_its_scale() {
    let p = place(
        BoundsType::Stretch,
        100.0,
        100.0,
        0.0,
        0.0,
        1.0,
        1.0,
        0,
        0,
        CENTER,
        false,
    );
    expect_placed(
        p,
        -50.0,
        -50.0,
        1.0,
        1.0,
        100,
        100,
        7,
        7,
        7,
        7,
        "empty stretch",
    );
}

#[test]
fn test_scene_bounds_place_max_only_shrinks_without_cropping() {
    let p = place(
        BoundsType::MaxOnly,
        100.0,
        100.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        true,
    );
    expect_placed(p, 0.0, -25.0, 0.5, 0.5, 100, 100, 7, 7, 7, 7, "max only");
}

#[test]
fn test_scene_bounds_place_scale_to_height_crops_the_width() {
    let scale = 0.4;

    let p = place(
        BoundsType::ScaleToHeight,
        50.0,
        40.0,
        0.0,
        0.0,
        1.0,
        1.0,
        200,
        100,
        CENTER,
        true,
    );
    expect_placed(p, 0.0, 0.0, scale, scale, 50, 40, 38, 7, 38, 7, "to height");
}

#[test]
fn test_scene_bounds_place_scale_to_width_crops_the_height() {
    let p = place(
        BoundsType::ScaleToWidth,
        50.0,
        20.0,
        1.0,
        2.0,
        1.0,
        1.0,
        100,
        100,
        CENTER,
        true,
    );
    expect_placed(p, 1.0, 0.0, 0.5, 0.5, 50, 20, 7, 30, 7, 30, "to width");
}
