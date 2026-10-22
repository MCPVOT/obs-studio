//! Original placement math and real obs.h crop layout.
use crate::vec2::OracleVec2;
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct OracleCrop {
    pub left: i32,
    pub top: i32,
    pub right: i32,
    pub bottom: i32,
}
unsafe extern "C" {
    pub fn oracle_add_alignment(v: *mut OracleVec2, align: u32, cx: i32, cy: i32);
    pub fn oracle_obs_bounds_place(
        kind: i32,
        bounds: *const OracleVec2,
        origin: *mut OracleVec2,
        scale: *mut OracleVec2,
        cx: *mut u32,
        cy: *mut u32,
        align: u32,
        crop_to_bounds: bool,
        crop: *mut OracleCrop,
    );
    pub fn oracle_scene_crop_layout(field: u32) -> usize;
}
