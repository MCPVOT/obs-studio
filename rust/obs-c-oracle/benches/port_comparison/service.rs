//! #1899, #1913: the obs-service getters and the settings and identity
//! functions, the obs-output shims against the C, on the same service.

// The oracle's obs_data_* and obs_properties_* calls link to these.
use obs_data as _;
use obs_properties as _;

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
unsafe extern "C" fn apply_encoder(_: *mut c_void, _: *mut c_void, _: *mut c_void) {}
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
        apply_encoder_settings: Some(apply_encoder),
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

    // SAFETY: a static name; the settings stay NULL (not read below).
    unsafe {
        c::oracle_service_set_context(service, c"bench".as_ptr().cast_mut(), ptr::null_mut())
    };
    // SAFETY: the table outlives the lookups; reset below.
    unsafe { c::oracle_service_set_types((&raw const info).cast(), 1) };
    let id = c"bench_service".as_ptr();
    let video = ptr::without_provenance_mut(0x1d);

    // SAFETY, each call: a live service and a registered id.
    pair(
        criterion,
        "service/get_name",
        || unsafe { rust::obs_service_get_name(black_box(s)) },
        || unsafe { c::oracle_obs_service_get_name(black_box(s)) },
    );
    pair(
        criterion,
        "service/get_id",
        || unsafe { rust::obs_service_get_id(black_box(s)) },
        || unsafe { c::oracle_obs_service_get_id(black_box(s)) },
    );
    pair(
        criterion,
        "service/get_display_name",
        || unsafe { rust::obs_service_get_display_name(black_box(id)) },
        || unsafe { c::oracle_obs_service_get_display_name(black_box(id)) },
    );
    pair(
        criterion,
        "service/apply_encoder_settings",
        || unsafe {
            rust::obs_service_apply_encoder_settings(black_box(service), video, ptr::null_mut())
        },
        || unsafe {
            c::oracle_obs_service_apply_encoder_settings(black_box(service), video, ptr::null_mut())
        },
    );

    // SAFETY: no types; from oracle_service_new, freed once.
    unsafe {
        c::oracle_service_set_types(ptr::null(), 0);
        c::oracle_service_free(service);
    }
}
