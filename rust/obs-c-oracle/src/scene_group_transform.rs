//! `oracle_*` build of `libobs/obs-scene-group-transform.c.h`
//! (oracle/scene_group_transform.c).

use super::graphics_math::OracleMatrix4;
use super::vec2::OracleVec2;

unsafe extern "C" {
    pub fn oracle_obs_scene_transform_val(dst: *mut OracleVec2, transform: *const OracleMatrix4);

    pub fn oracle_obs_scene_group_transform(
        transform: *const OracleMatrix4,
        pos: *mut OracleVec2,
        scale: *mut OracleVec2,
        rot: *mut f32,
        rot_delta: f32,
    );
}
