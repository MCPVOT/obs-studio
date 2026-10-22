//! Internal C ABI for scene bounds placement. Symbols remain hidden.
use crate::bounds::BoundsType;
use crate::bounds_place::{self, Crop, Placement};
use core::ffi::c_int;
use core::ptr;
use obs_graphics::ffi::vec2::vec2;

/// Layout of obs_sceneitem_crop from obs.h, measured by the C oracle.
#[repr(C)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct SceneItemCrop {
    pub left: c_int,
    pub top: c_int,
    pub right: c_int,
    pub bottom: c_int,
}
impl From<SceneItemCrop> for Crop {
    fn from(v: SceneItemCrop) -> Self {
        Self {
            left: v.left,
            top: v.top,
            right: v.right,
            bottom: v.bottom,
        }
    }
}
impl From<Crop> for SceneItemCrop {
    fn from(v: Crop) -> Self {
        Self {
            left: v.left,
            top: v.top,
            right: v.right,
            bottom: v.bottom,
        }
    }
}

/// # Safety
/// `v` must point to a readable and writable vec2.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn add_alignment(v: *mut vec2, align: u32, cx: c_int, cy: c_int) {
    // SAFETY: the caller provides a live readable and writable vector.
    unsafe {
        ptr::write(
            v,
            bounds_place::add_alignment(ptr::read(v).into(), align, cx, cy).into(),
        )
    };
}

/// # Safety
/// All pointers must be readable, and all except bounds writable. Output
/// pointers refer to distinct fields, as in calculate_bounds_data.
#[unsafe(no_mangle)]
#[allow(clippy::too_many_arguments)]
pub unsafe extern "C" fn obs_bounds_place(
    kind: c_int,
    bounds: *const vec2,
    origin: *mut vec2,
    scale: *mut vec2,
    cx: *mut u32,
    cy: *mut u32,
    align: u32,
    crop_to_bounds: bool,
    crop: *mut SceneItemCrop,
) {
    // SAFETY: the caller provides live readable fields; read before writing outputs.
    let (bounds, item) = unsafe {
        (
            ptr::read(bounds).into(),
            Placement {
                origin: ptr::read(origin).into(),
                scale: ptr::read(scale).into(),
                cx: ptr::read(cx),
                cy: ptr::read(cy),
                crop: ptr::read(crop).into(),
            },
        )
    };
    let out = bounds_place::bounds_place(
        BoundsType::from_c(kind),
        bounds,
        item,
        align,
        crop_to_bounds,
    );
    // SAFETY: caller guarantees writable, distinct output fields.
    unsafe {
        ptr::write(origin, out.origin.into());
        ptr::write(scale, out.scale.into());
        ptr::write(cx, out.cx);
        ptr::write(cy, out.cy);
        ptr::write(crop, out.crop.into());
    }
}
