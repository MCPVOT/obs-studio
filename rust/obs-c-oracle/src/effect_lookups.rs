//! `oracle_*` build of `libobs/graphics/effect-lookups.c.h`
//! (oracle/effect_lookups.c), plus the struct layouts from `graphics/effect.h`.

use core::ffi::{c_char, c_void};

unsafe extern "C" {
    pub fn oracle_gs_effect_get_technique(
        effect: *const c_void,
        name: *const c_char,
    ) -> *mut c_void;
    pub fn oracle_gs_effect_get_current_technique(effect: *const c_void) -> *mut c_void;
    pub fn oracle_gs_effect_get_num_params(effect: *const c_void) -> usize;
    pub fn oracle_gs_effect_get_param_by_idx(effect: *const c_void, param: usize) -> *mut c_void;
    pub fn oracle_gs_effect_get_param_by_name(
        effect: *const c_void,
        name: *const c_char,
    ) -> *mut c_void;
    pub fn oracle_gs_param_get_num_annotations(param: *const c_void) -> usize;
    pub fn oracle_gs_param_get_annotation_by_idx(
        param: *const c_void,
        annotation: usize,
    ) -> *mut c_void;
    pub fn oracle_gs_param_get_annotation_by_name(
        param: *const c_void,
        name: *const c_char,
    ) -> *mut c_void;
    pub fn oracle_gs_technique_get_pass_by_idx(
        technique: *const c_void,
        pass: usize,
    ) -> *mut c_void;
    pub fn oracle_gs_technique_get_pass_by_name(
        technique: *const c_void,
        name: *const c_char,
    ) -> *mut c_void;
    pub fn oracle_gs_effect_get_viewproj_matrix(effect: *const c_void) -> *mut c_void;
    pub fn oracle_gs_effect_get_world_matrix(effect: *const c_void) -> *mut c_void;
    pub fn oracle_gs_effect_get_param_info(param: *const c_void, info: *mut c_void);

    pub fn oracle_effect_param_size() -> usize;
    pub fn oracle_effect_param_align() -> usize;
    pub fn oracle_effect_param_offset_name() -> usize;
    pub fn oracle_effect_param_offset_section() -> usize;
    pub fn oracle_effect_param_offset_type() -> usize;
    pub fn oracle_effect_param_offset_changed() -> usize;
    pub fn oracle_effect_param_offset_cur_val() -> usize;
    pub fn oracle_effect_param_offset_default_val() -> usize;
    pub fn oracle_effect_param_offset_effect() -> usize;
    pub fn oracle_effect_param_offset_next_sampler() -> usize;
    pub fn oracle_effect_param_offset_annotations() -> usize;

    pub fn oracle_effect_pass_size() -> usize;
    pub fn oracle_effect_pass_align() -> usize;
    pub fn oracle_effect_pass_offset_name() -> usize;
    pub fn oracle_effect_pass_offset_section() -> usize;
    pub fn oracle_effect_pass_offset_vertshader() -> usize;
    pub fn oracle_effect_pass_offset_pixelshader() -> usize;
    pub fn oracle_effect_pass_offset_vertshader_params() -> usize;
    pub fn oracle_effect_pass_offset_pixelshader_params() -> usize;

    pub fn oracle_effect_technique_size() -> usize;
    pub fn oracle_effect_technique_align() -> usize;
    pub fn oracle_effect_technique_offset_name() -> usize;
    pub fn oracle_effect_technique_offset_section() -> usize;
    pub fn oracle_effect_technique_offset_effect() -> usize;
    pub fn oracle_effect_technique_offset_passes() -> usize;

    pub fn oracle_effect_size() -> usize;
    pub fn oracle_effect_align() -> usize;
    pub fn oracle_effect_offset_processing() -> usize;
    pub fn oracle_effect_offset_cached() -> usize;
    pub fn oracle_effect_offset_effect_path() -> usize;
    pub fn oracle_effect_offset_effect_dir() -> usize;
    pub fn oracle_effect_offset_params() -> usize;
    pub fn oracle_effect_offset_techniques() -> usize;
    pub fn oracle_effect_offset_cur_technique() -> usize;
    pub fn oracle_effect_offset_cur_pass() -> usize;
    pub fn oracle_effect_offset_view_proj() -> usize;
    pub fn oracle_effect_offset_world() -> usize;
    pub fn oracle_effect_offset_scale() -> usize;
    pub fn oracle_effect_offset_graphics() -> usize;
    pub fn oracle_effect_offset_next() -> usize;
    pub fn oracle_effect_offset_loop_pass() -> usize;
    pub fn oracle_effect_offset_looping() -> usize;

    pub fn oracle_effect_param_info_size() -> usize;
    pub fn oracle_effect_param_info_align() -> usize;
    pub fn oracle_effect_param_info_offset_name() -> usize;
    pub fn oracle_effect_param_info_offset_type() -> usize;
}
