//! Placement, crop, and flip adjustments for scene bounds.
use crate::bounds::BoundsType;
use obs_graphics::vec2::Vec2;

pub const LEFT: u32 = 1;
pub const RIGHT: u32 = 2;
pub const TOP: u32 = 4;
pub const BOTTOM: u32 = 8;
pub const CENTER: u32 = 0;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct Crop {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Placement {
    pub origin: Vec2,
    pub scale: Vec2,
    pub cx: u32,
    pub cy: u32,
    pub crop: Crop,
}

/// Apply alignment with C's integer division toward zero. RIGHT and BOTTOM win.
#[must_use]
pub fn add_alignment(mut v: Vec2, align: u32, cx: i32, cy: i32) -> Vec2 {
    if align & RIGHT != 0 {
        v.x += cx as f32;
    } else if align & LEFT == 0 {
        v.x += (cx / 2) as f32;
    }
    if align & BOTTOM != 0 {
        v.y += cy as f32;
    } else if align & TOP == 0 {
        v.y += (cy / 2) as f32;
    }
    v
}

/// Fit, align, optionally crop, and retain the item's position when flipped.
/// C float-to-integer conversions are preserved for finite representable inputs.
/// Outside C's defined conversion domain Rust saturates, avoiding undefined behavior.
#[must_use]
pub fn bounds_place(
    kind: BoundsType,
    bounds: Vec2,
    mut item: Placement,
    align: u32,
    crop_to_bounds: bool,
) -> Placement {
    item.scale = crate::bounds::bounds_fit_scale(kind, bounds, item.cx, item.cy, item.scale);
    let width = item.cx as f32 * item.scale.x;
    let height = item.cy as f32 * item.scale.y;
    let width_diff = bounds.x - width.abs();
    let height_diff = bounds.y - height.abs();
    item.cx = bounds.x as u32;
    item.cy = bounds.y as u32;
    item.origin = add_alignment(
        item.origin,
        align,
        (-width_diff) as i32,
        (-height_diff) as i32,
    );
    let active = crop_to_bounds
        && matches!(
            kind,
            BoundsType::ScaleOuter | BoundsType::ScaleToHeight | BoundsType::ScaleToWidth
        );
    if active && (f64::from(width_diff) < -0.1 || f64::from(height_diff) < -0.1) {
        let crop_width = f64::from(width_diff) < -0.1;
        let (diff, scale, mut origin, flipped, mask) = if crop_width {
            (
                width_diff,
                item.scale.x,
                item.origin.x,
                width < 0.0,
                LEFT | RIGHT,
            )
        } else {
            (
                height_diff,
                item.scale.y,
                item.origin.y,
                height < 0.0,
                TOP | BOTTOM,
            )
        };
        let crop_align = align & mask;
        let overdraw = (diff / scale).abs();
        let overdraw_tl = if crop_align & (TOP | LEFT) != 0 {
            0.0
        } else if crop_align & (BOTTOM | RIGHT) != 0 {
            overdraw
        } else {
            overdraw / 2.0
        };
        let overdraw_br = overdraw - overdraw_tl;
        let (tl, br) = if flipped {
            if crop_align == CENTER {
                origin *= 2.0;
            } else if crop_align & (TOP | LEFT) != 0 {
                origin -= diff;
            }
            (overdraw_br.round() as i32, overdraw_tl.round() as i32)
        } else {
            origin = 0.0;
            (overdraw_tl.round() as i32, overdraw_br.round() as i32)
        };
        if crop_width {
            item.crop.left = tl;
            item.crop.right = br;
            item.origin.x = origin;
        } else {
            item.crop.top = tl;
            item.crop.bottom = br;
            item.origin.y = origin;
        }
    }
    item.origin.x += if width < 0.0 { width } else { 0.0 };
    item.origin.y += if height < 0.0 { height } else { 0.0 };
    item
}
