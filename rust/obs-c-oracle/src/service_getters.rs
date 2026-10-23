//! `oracle_*` build of `libobs/obs-service-getters.c.h`
//! (oracle/service_getters.c): the nine service getters, renamed, plus a
//! constructor for the `struct obs_service` both the C and the Rust shims
//! read, and the layout of `struct obs_service_info`.

use core::ffi::{c_char, c_int, c_void};

unsafe extern "C" {
    /// A zeroed `struct obs_service` with `info` copied and `data` as its
    /// plugin data. Free with [`oracle_service_free`].
    pub fn oracle_service_new(info: *const c_void, data: *mut c_void) -> *mut c_void;
    pub fn oracle_service_free(service: *mut c_void);

    pub fn oracle_obs_service_get_protocol(service: *const c_void) -> *const c_char;
    pub fn oracle_obs_service_get_connect_info(service: *const c_void, kind: u32) -> *const c_char;
    pub fn oracle_obs_service_can_try_to_connect(service: *const c_void) -> bool;
    pub fn oracle_obs_service_get_preferred_output_type(service: *const c_void) -> *const c_char;
    pub fn oracle_obs_service_get_max_fps(service: *const c_void, fps: *mut c_int);
    pub fn oracle_obs_service_get_max_bitrate(
        service: *const c_void,
        video: *mut c_int,
        audio: *mut c_int,
    );
    pub fn oracle_obs_service_get_supported_resolutions(
        service: *const c_void,
        resolutions: *mut *mut c_void,
        count: *mut usize,
    );
    pub fn oracle_obs_service_get_supported_video_codecs(
        service: *const c_void,
    ) -> *mut *const c_char;
    pub fn oracle_obs_service_get_supported_audio_codecs(
        service: *const c_void,
    ) -> *mut *const c_char;

    pub fn oracle_service_resolution_size() -> usize;
    pub fn oracle_service_resolution_align() -> usize;
    pub fn oracle_service_resolution_offset_cy() -> usize;
    pub fn oracle_service_info_size() -> usize;
    pub fn oracle_service_info_align() -> usize;
    pub fn oracle_service_info_offset_id() -> usize;
    pub fn oracle_service_info_offset_get_name() -> usize;
    pub fn oracle_service_info_offset_create() -> usize;
    pub fn oracle_service_info_offset_destroy() -> usize;
    pub fn oracle_service_info_offset_activate() -> usize;
    pub fn oracle_service_info_offset_deactivate() -> usize;
    pub fn oracle_service_info_offset_update() -> usize;
    pub fn oracle_service_info_offset_get_defaults() -> usize;
    pub fn oracle_service_info_offset_get_properties() -> usize;
    pub fn oracle_service_info_offset_initialize() -> usize;
    pub fn oracle_service_info_offset_get_url() -> usize;
    pub fn oracle_service_info_offset_get_key() -> usize;
    pub fn oracle_service_info_offset_get_username() -> usize;
    pub fn oracle_service_info_offset_get_password() -> usize;
    pub fn oracle_service_info_offset_deprecated_1() -> usize;
    pub fn oracle_service_info_offset_apply_encoder_settings() -> usize;
    pub fn oracle_service_info_offset_type_data() -> usize;
    pub fn oracle_service_info_offset_free_type_data() -> usize;
    pub fn oracle_service_info_offset_get_output_type() -> usize;
    pub fn oracle_service_info_offset_get_supported_resolutions() -> usize;
    pub fn oracle_service_info_offset_get_max_fps() -> usize;
    pub fn oracle_service_info_offset_get_max_bitrate() -> usize;
    pub fn oracle_service_info_offset_get_supported_video_codecs() -> usize;
    pub fn oracle_service_info_offset_get_protocol() -> usize;
    pub fn oracle_service_info_offset_get_supported_audio_codecs() -> usize;
    pub fn oracle_service_info_offset_get_connect_info() -> usize;
    pub fn oracle_service_info_offset_can_try_to_connect() -> usize;
}
