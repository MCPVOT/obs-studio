//! `oracle_*` build of `libobs/obs-scene-coords.c.h`
//! (oracle/scene_coords.c).

use crate::vec2::OracleVec2;

unsafe extern "C" {
    pub fn oracle_obs_scene_coords_nudge(is_group: bool, parent_is_group: bool) -> bool;
    pub fn oracle_obs_scene_coords_keep_scale(is_group: bool, is_scene: bool) -> bool;

    pub fn oracle_obs_scene_nudge_abs(dst: *mut OracleVec2, v: *const OracleVec2);
    pub fn oracle_obs_scene_pos_to_absolute(
        dst: *mut OracleVec2,
        v: *const OracleVec2,
        canvas_x: f32,
        canvas_y: f32,
        nudge: bool,
    );
    pub fn oracle_obs_scene_pos_from_absolute(
        dst: *mut OracleVec2,
        v: *const OracleVec2,
        canvas_x: f32,
        canvas_y: f32,
    );
    pub fn oracle_obs_scene_size_to_absolute(
        dst: *mut OracleVec2,
        v: *const OracleVec2,
        canvas_y: f32,
        nudge: bool,
    );
    pub fn oracle_obs_scene_size_from_absolute(
        dst: *mut OracleVec2,
        v: *const OracleVec2,
        canvas_y: f32,
    );
    pub fn oracle_obs_scene_item_canvas_scale(
        dst: *mut OracleVec2,
        scale: *const OracleVec2,
        canvas_y: f32,
        scale_ref_y: f32,
    );
    pub fn oracle_obs_scene_item_relative_scale(
        dst: *mut OracleVec2,
        v: *const OracleVec2,
        canvas_y: f32,
        scale_ref_y: f32,
    );
}
