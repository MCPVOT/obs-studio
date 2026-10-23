//! #1899: the obs-service getters, the obs-output shims against the C, on
//! the same service with every optional callback filled.

use super::pair;
use criterion::Criterion;
use obs_c_oracle::service_getters as c;
use obs_output::ffi::service::{self as rust, ObsServiceInfo, ObsServiceResolution};
use std::ffi::{c_char, c_int, c_void};
use std::hint::black_box;
use std::ptr;

unsafe extern "C" fn get_name(_: *mut c_void) -> *const c_char {
    c"bench".as_ptr()
}
unsafe extern "C" fn create(_: *mut c_void, _: *mut c_void) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn destroy(_: *mut c_void) {}
unsafe extern "C" fn protocol(_: *mut c_void) -> *const c_char {
    c"SRT".as_ptr()
}
unsafe extern "C" fn connect_info(_: *mut c_void, kind: u32) -> *const c_char {
    if kind == 0 {
        c"srt://example.invalid".as_ptr()
    } else {
        ptr::null()
    }
}
unsafe extern "C" fn max_bitrate(_: *mut c_void, video: *mut c_int, _: *mut c_int) {
    // SAFETY: the getter passes a writable video bitrate here.
    unsafe { *video = 6000 };
}

pub fn benchmarks(criterion: &mut Criterion) {
    let info = ObsServiceInfo {
        id: c"bench_service".as_ptr(),
        get_name: Some(get_name),
        create: Some(create),
        destroy: Some(destroy),
        activate: None,
        deactivate: None,
        update: None,
        get_defaults: None,
        get_properties: None,
        initialize: None,
        get_url: None,
        get_key: None,
        get_username: None,
        get_password: None,
        deprecated_1: None,
        apply_encoder_settings: None,
        type_data: ptr::null_mut(),
        free_type_data: None,
        get_output_type: None,
        get_supported_resolutions: None::<
            unsafe extern "C" fn(*mut c_void, *mut *mut ObsServiceResolution, *mut usize),
        >,
        get_max_fps: None,
        get_max_bitrate: Some(max_bitrate),
        get_supported_video_codecs: None,
        get_protocol: Some(protocol),
        get_supported_audio_codecs: None,
        get_connect_info: Some(connect_info),
        can_try_to_connect: None,
    };
    // SAFETY: a full table; the service lives until the end of this function.
    let service = unsafe { c::oracle_service_new((&raw const info).cast(), ptr::null_mut()) };
    let s = service.cast_const();

    // SAFETY, each call: a live service from oracle_service_new.
    pair(
        criterion,
        "service/get_protocol",
        || unsafe { rust::obs_service_get_protocol(black_box(s)) },
        || unsafe { c::oracle_obs_service_get_protocol(black_box(s)) },
    );
    pair(
        criterion,
        "service/get_connect_info",
        || unsafe { rust::obs_service_get_connect_info(black_box(s), black_box(0)) },
        || unsafe { c::oracle_obs_service_get_connect_info(black_box(s), black_box(0)) },
    );
    pair(
        criterion,
        "service/can_try_to_connect",
        || unsafe { rust::obs_service_can_try_to_connect(black_box(s)) },
        || unsafe { c::oracle_obs_service_can_try_to_connect(black_box(s)) },
    );
    pair(
        criterion,
        "service/get_max_bitrate",
        || {
            let (mut v, mut a) = (0, 0);
            unsafe { rust::obs_service_get_max_bitrate(black_box(s), &mut v, &mut a) };
            (v, a)
        },
        || {
            let (mut v, mut a) = (0, 0);
            unsafe { c::oracle_obs_service_get_max_bitrate(black_box(s), &mut v, &mut a) };
            (v, a)
        },
    );

    // SAFETY: from oracle_service_new, freed once.
    unsafe { c::oracle_service_free(service) };
}
