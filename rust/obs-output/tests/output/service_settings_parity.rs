//! Tier 3 for the service settings, properties and identity functions
//! (#1913): the Rust shims in obs_output::ffi::service against the original
//! C (rust/obs-c-oracle/oracle/service_getters.c), on the same
//! `struct obs_service` and the same `find_service()` table.
//!
//! Every combination of the 4 optional callbacks these reach (get_defaults,
//! get_properties, update, apply_encoder_settings), with and without type
//! data, runs every function on both sides; they must return the same value
//! (or, for a new object, the same contents), leave the service's settings
//! the same and make the same plugin calls with the same arguments.
//!
//! Not compared: the NULL-parameter debug log lines (Tier 1 checks their
//! function names).

use core::cell::RefCell;
use core::ffi::{CStr, c_char, c_void};
use core::ptr;

use obs_c_oracle::service_getters as c;
use obs_output::ffi::service::{self as rust, ObsServiceInfo};

unsafe extern "C" {
    fn obs_data_create() -> *mut c_void;
    fn obs_data_release(data: *mut c_void);
    fn obs_data_set_int(data: *mut c_void, name: *const c_char, val: i64);
    fn obs_data_get_int(data: *mut c_void, name: *const c_char) -> i64;
    fn obs_data_set_default_int(data: *mut c_void, name: *const c_char, val: i64);
    fn obs_data_has_user_value(data: *mut c_void, name: *const c_char) -> bool;
    fn obs_properties_create() -> *mut c_void;
    fn obs_properties_add_int(
        props: *mut c_void,
        name: *const c_char,
        desc: *const c_char,
        min: i32,
        max: i32,
        step: i32,
    ) -> *mut c_void;
    fn obs_property_set_modified_callback(
        p: *mut c_void,
        cb: unsafe extern "C" fn(*mut c_void, *mut c_void, *mut c_void) -> bool,
    );
    fn obs_properties_destroy(props: *mut c_void);
}

thread_local! {
    static TRACE: RefCell<Vec<String>> = const { RefCell::new(Vec::new()) };
}

fn trace(event: String) {
    TRACE.with(|t| t.borrow_mut().push(event));
}

fn take_trace() -> Vec<String> {
    TRACE.with(|t| core::mem::take(&mut *t.borrow_mut()))
}

const BITRATE: &CStr = c"bitrate";

fn bitrate(data: *mut c_void) -> i64 {
    // SAFETY: a live obs_data_t.
    unsafe { obs_data_get_int(data, BITRATE.as_ptr()) }
}

unsafe extern "C" fn get_name(type_data: *mut c_void) -> *const c_char {
    trace(format!("get_name {type_data:p}"));
    c"parity".as_ptr()
}
unsafe extern "C" fn create(_: *mut c_void, _: *mut c_void) -> *mut c_void {
    ptr::null_mut()
}
unsafe extern "C" fn destroy(_: *mut c_void) {}
unsafe extern "C" fn protocol(_: *mut c_void) -> *const c_char {
    c"RTMP".as_ptr()
}
unsafe extern "C" fn get_defaults(settings: *mut c_void) {
    trace("get_defaults".into());
    // SAFETY: the object the function created.
    unsafe { obs_data_set_default_int(settings, BITRATE.as_ptr(), 2500) };
}
unsafe extern "C" fn modified(_: *mut c_void, _: *mut c_void, settings: *mut c_void) -> bool {
    trace(format!("modified bitrate={}", bitrate(settings)));
    false
}
unsafe extern "C" fn get_properties(data: *mut c_void) -> *mut c_void {
    trace(format!("get_properties {data:p}"));
    // SAFETY: libobs properties calls on a new object.
    unsafe {
        let props = obs_properties_create();
        let p = obs_properties_add_int(props, BITRATE.as_ptr(), c"Bitrate".as_ptr(), 0, 100_000, 1);
        obs_property_set_modified_callback(p, modified);
        props
    }
}
unsafe extern "C" fn update(data: *mut c_void, settings: *mut c_void) {
    trace(format!("update {data:p} bitrate={}", bitrate(settings)));
}
unsafe extern "C" fn apply_encoder(data: *mut c_void, video: *mut c_void, audio: *mut c_void) {
    trace(format!("apply_encoder {data:p} {video:p} {audio:p}"));
}

static ID: &CStr = c"parity_service";
static mut TYPE_DATA: u8 = 0;

fn table(mask: u32) -> ObsServiceInfo {
    let has = |bit: u32| mask & (1 << bit) != 0;
    ObsServiceInfo {
        id: ID.as_ptr(),
        get_name: Some(get_name),
        create: Some(create),
        destroy: Some(destroy),
        activate: None,
        deactivate: None,
        update: has(0).then_some(update as _),
        get_defaults: has(1).then_some(get_defaults as _),
        get_properties: has(2).then_some(get_properties as _),
        initialize: None,
        get_url: None,
        get_key: None,
        get_username: None,
        get_password: None,
        deprecated_1: None,
        apply_encoder_settings: has(3).then_some(apply_encoder as _),
        type_data: if has(4) {
            (&raw mut TYPE_DATA).cast()
        } else {
            ptr::null_mut()
        },
        free_type_data: None,
        get_output_type: None,
        get_supported_resolutions: None,
        get_max_fps: None,
        get_max_bitrate: None,
        get_supported_video_codecs: None,
        get_protocol: Some(protocol),
        get_supported_audio_codecs: None,
        get_connect_info: None,
        can_try_to_connect: None,
    }
}

/// A service of `info` with its own settings (bitrate 1000).
struct Svc {
    ptr: *mut c_void,
    settings: *mut c_void,
}

static SVC_NAME: &CStr = c"svc";
const PLUGIN_DATA: *mut c_void = ptr::without_provenance_mut(0x5e7);

impl Svc {
    fn new(info: &ObsServiceInfo) -> Self {
        // SAFETY: oracle constructors and libobs data calls.
        unsafe {
            let settings = obs_data_create();
            obs_data_set_int(settings, BITRATE.as_ptr(), 1000);
            let ptr = c::oracle_service_new(ptr::from_ref(info).cast(), PLUGIN_DATA);
            c::oracle_service_set_context(ptr, SVC_NAME.as_ptr().cast_mut(), settings);
            Svc { ptr, settings }
        }
    }
}

impl Drop for Svc {
    fn drop(&mut self) {
        // SAFETY: made by new().
        unsafe {
            c::oracle_service_free(self.ptr);
            obs_data_release(self.settings);
        }
    }
}

/// What a returned `obs_data_t` holds, then releases it.
fn data_contents(data: *mut c_void) -> Option<(i64, bool)> {
    if data.is_null() {
        return None;
    }
    // SAFETY: a live object the function returned with a ref.
    unsafe {
        let out = (
            bitrate(data),
            obs_data_has_user_value(data, BITRATE.as_ptr()),
        );
        obs_data_release(data);
        Some(out)
    }
}

fn props_present(props: *mut c_void) -> bool {
    // SAFETY: a properties object the function returned.
    unsafe { obs_properties_destroy(props) };
    !props.is_null()
}

#[test]
fn by_id_functions_match_the_c() {
    let _types = super::SERVICE_GLOBALS
        .lock()
        .unwrap_or_else(|e| e.into_inner());
    for mask in 0..32 {
        let types = [table(mask)];
        // SAFETY: the table outlives every call below; reset after.
        unsafe { c::oracle_service_set_types(types.as_ptr().cast(), 1) };
        for id in [ID, c"parity_missing"] {
            let id = id.as_ptr();
            // SAFETY: a C string id; libobs data and properties calls.
            unsafe {
                take_trace();
                let cn = c::oracle_obs_service_get_display_name(id);
                let ct = take_trace();
                let rn = rust::obs_service_get_display_name(id);
                assert_eq!((cn, ct), (rn, take_trace()), "display_name mask {mask:#x}");

                let cd = data_contents(c::oracle_obs_service_defaults(id));
                let ct = take_trace();
                let rd = data_contents(rust::obs_service_defaults(id));
                assert_eq!((cd, ct), (rd, take_trace()), "defaults mask {mask:#x}");

                let cp = props_present(c::oracle_obs_get_service_properties(id));
                let ct = take_trace();
                let rp = props_present(rust::obs_get_service_properties(id));
                assert_eq!((cp, ct), (rp, take_trace()), "properties mask {mask:#x}");
            }
        }
        // SAFETY: no types.
        unsafe { c::oracle_service_set_types(ptr::null(), 0) };
    }
}

#[test]
fn service_functions_match_the_c() {
    for mask in 0..32 {
        let info = table(mask);
        let (cs, rs) = (Svc::new(&info), Svc::new(&info));
        // SAFETY: live services; libobs data calls on live objects.
        unsafe {
            take_trace();
            for (cp, rp) in [(cs.ptr, rs.ptr), (ptr::null_mut(), ptr::null_mut())] {
                assert_eq!(
                    c::oracle_obs_service_get_name(cp),
                    rust::obs_service_get_name(rp)
                );
                assert_eq!(
                    c::oracle_obs_service_get_type(cp),
                    rust::obs_service_get_type(rp)
                );
                assert_eq!(
                    c::oracle_obs_service_get_id(cp),
                    rust::obs_service_get_id(rp)
                );
                assert_eq!(
                    c::oracle_obs_service_get_type_data(cp),
                    rust::obs_service_get_type_data(rp)
                );

                let cpr = props_present(c::oracle_obs_service_properties(cp));
                let ct = take_trace();
                let rpr = props_present(rust::obs_service_properties(rp));
                assert_eq!((cpr, ct), (rpr, take_trace()), "properties mask {mask:#x}");

                let new = obs_data_create();
                obs_data_set_int(new, BITRATE.as_ptr(), 4000);
                for settings in [new, ptr::null_mut()] {
                    c::oracle_obs_service_update(cp, settings);
                    let ct = take_trace();
                    rust::obs_service_update(rp, settings);
                    assert_eq!(ct, take_trace(), "update mask {mask:#x}");
                }
                obs_data_release(new);
                assert_eq!(bitrate(cs.settings), bitrate(rs.settings));

                let cg = c::oracle_obs_service_get_settings(cp);
                let rg = rust::obs_service_get_settings(rp);
                assert_eq!(cg.is_null(), rg.is_null());
                if !cg.is_null() {
                    assert_eq!((cg, rg), (cs.settings, rs.settings));
                    obs_data_release(cg);
                    obs_data_release(rg);
                }

                let v = ptr::without_provenance_mut(0x1d);
                let a = ptr::without_provenance_mut(0xa0);
                for (video, audio) in [
                    (v, a),
                    (v, ptr::null_mut()),
                    (ptr::null_mut(), a),
                    (ptr::null_mut(), ptr::null_mut()),
                ] {
                    c::oracle_obs_service_apply_encoder_settings(cp, video, audio);
                    let ct = take_trace();
                    rust::obs_service_apply_encoder_settings(rp, video, audio);
                    assert_eq!(ct, take_trace(), "apply_encoder mask {mask:#x}");
                }
            }
        }
    }
}
