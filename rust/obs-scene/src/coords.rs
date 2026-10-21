//! Safe core of `libobs/obs-scene-coords.c.h`.
//!
//! The scene still reads the canvas and still decides the nudge and
//! keep-scale flags. This is only the math. Each expression is the C one,
//! on `f32`, in the same order. C writes an integer `2` and divides by
//! `2`; both promote to float. `floor` is [`Vec2::floor`], the same
//! `floorf` the graphics port uses.

use obs_graphics::vec2::Vec2;

/// `obs_scene_coords_nudge`: true unless the item is a group or its parent is.
#[must_use]
pub fn coords_nudge(is_group: bool, parent_is_group: bool) -> bool {
    !is_group && !parent_is_group
}

/// `obs_scene_coords_keep_scale`: true for a group or a nested scene.
#[must_use]
pub fn coords_keep_scale(is_group: bool, is_scene: bool) -> bool {
    is_group || is_scene
}

/// `obs_scene_nudge_abs`: `floor(v * 2 + 0.5) / 2` on each axis.
///
/// A value already on a half stays. Positive ties go up. A negative tie
/// goes toward zero. Characterized, not endorsed.
#[must_use]
pub fn nudge_abs(v: Vec2) -> Vec2 {
    let scaled = Vec2::new(v.x * 2.0 + 0.5, v.y * 2.0 + 0.5);
    let floored = scaled.floor();
    Vec2::new(floored.x / 2.0, floored.y / 2.0)
}

/// `obs_scene_pos_to_absolute`. Height is `[-1, 1]`, centered.
#[must_use]
pub fn pos_to_absolute(v: Vec2, canvas_x: f32, canvas_y: f32, nudge: bool) -> Vec2 {
    let dst = Vec2::new(
        (v.x * canvas_y + canvas_x) / 2.0,
        (v.y * canvas_y + canvas_y) / 2.0,
    );
    if nudge { nudge_abs(dst) } else { dst }
}

/// `obs_scene_pos_from_absolute`. Does not nudge.
#[must_use]
pub fn pos_from_absolute(v: Vec2, canvas_x: f32, canvas_y: f32) -> Vec2 {
    Vec2::new(
        (2.0 * v.x - canvas_x) / canvas_y,
        2.0 * v.y / canvas_y - 1.0,
    )
}

/// `obs_scene_size_to_absolute`. Canvas height is 2 in relative size.
#[must_use]
pub fn size_to_absolute(v: Vec2, canvas_y: f32, nudge: bool) -> Vec2 {
    let dst = Vec2::new((v.x * canvas_y) / 2.0, (v.y * canvas_y) / 2.0);
    if nudge { nudge_abs(dst) } else { dst }
}

/// `obs_scene_size_from_absolute`. Does not nudge.
#[must_use]
pub fn size_from_absolute(v: Vec2, canvas_y: f32) -> Vec2 {
    Vec2::new((2.0 * v.x) / canvas_y, 2.0 * v.y / canvas_y)
}

/// `obs_scene_item_canvas_scale`: `scale * (canvas_y / scale_ref_y)`.
#[must_use]
pub fn item_canvas_scale(scale: Vec2, canvas_y: f32, scale_ref_y: f32) -> Vec2 {
    let scale_factor = canvas_y / scale_ref_y;
    Vec2::new(scale.x * scale_factor, scale.y * scale_factor)
}

/// `obs_scene_item_relative_scale`: `v * (scale_ref_y / canvas_y)`.
#[must_use]
pub fn item_relative_scale(v: Vec2, canvas_y: f32, scale_ref_y: f32) -> Vec2 {
    let scale_factor = scale_ref_y / canvas_y;
    Vec2::new(v.x * scale_factor, v.y * scale_factor)
}
