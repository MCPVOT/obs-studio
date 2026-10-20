//! `oracle_*` build of `libobs/obs-scene-bounds.c.h`
//! (oracle/scene_bounds.c).

use core::ffi::c_int;

use crate::vec2::OracleVec2;

unsafe extern "C" {
    pub fn oracle_obs_bounds_fit_scale(
        type_: c_int,
        bounds: *const OracleVec2,
        cx: u32,
        cy: u32,
        scale: *mut OracleVec2,
    );
}
