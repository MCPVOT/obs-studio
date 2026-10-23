//! Safe core of the service capability and connect-info getters in
//! `libobs/obs-service.c.h` (#1899): `obs_service_get_protocol()`,
//! `_get_connect_info()`, `_can_try_to_connect()`,
//! `_get_preferred_output_type()`, `_get_max_fps()`, `_get_max_bitrate()`,
//! `_get_supported_resolutions()` and the supported codec lists.
//!
//! Each getter checks its arguments, writes its out-parameters' defaults and
//! hands the call to the plugin's optional callback in
//! `struct obs_service_info`. [`Callbacks`] is that callback table for one
//! service: a method returns `None` (or `false`) when the plugin left the
//! callback NULL. What the plugin returns is passed through untouched, so
//! the string and list types stay opaque.
//!
//! The order of checks and writes is the C's, including what
//! `test/cmocka/test_service_getters.c` marks "characterized, not
//! endorsed": `max_bitrate` zeroes both out-parameters before it checks the
//! service, while `max_fps` and `supported_resolutions` check first; the
//! codec getters take a service that is never null (the C dereferences it).
//! Null logging goes through [`NullLog`] with the C's function and parameter
//! names, `"obs_service_get_info"` for connect info included.

use core::ffi::CStr;

/// What the plugin's callbacks of one service do. `None` / `false`: the
/// callback is NULL.
pub trait Callbacks {
    /// `const char *`, passed through.
    type Str: Copy;
    /// `const char **`, passed through.
    type List: Copy;
    /// `struct obs_service_resolution *`, written by the plugin.
    type Resolutions;

    /// `get_protocol`, required at registration.
    fn protocol(&self) -> Self::Str;
    fn connect_info(&self, kind: u32) -> Option<Self::Str>;
    fn can_try_to_connect(&self) -> Option<bool>;
    fn output_type(&self) -> Option<Self::Str>;
    /// Calls `get_max_fps`; false when it is NULL.
    fn max_fps(&self, fps: &mut i32) -> bool;
    /// Calls `get_max_bitrate` with the caller's out-parameters, NULL ones
    /// included; false when it is NULL.
    fn max_bitrate(&self, video: Option<&mut i32>, audio: Option<&mut i32>) -> bool;
    /// Calls `get_supported_resolutions`; false when it is NULL.
    fn resolutions(&self, resolutions: &mut Self::Resolutions, count: &mut usize) -> bool;
    fn video_codecs(&self) -> Option<Self::List>;
    fn audio_codecs(&self) -> Option<Self::List>;
}

/// `obs_ptr_valid`'s debug line: `"%s: Null '%s' parameter"`.
pub trait NullLog {
    fn null_param(&self, func: &'static CStr, param: &'static CStr);
}

/// `obs_service_valid(service, func)`: the service, or the debug line.
fn valid<'a, S>(service: Option<&'a S>, log: &impl NullLog, func: &'static CStr) -> Option<&'a S> {
    if service.is_none() {
        log.null_param(func, c"service");
    }
    service
}

/// `obs_ptr_valid(ptr, func)` for an out-parameter named `param`.
fn valid_out<'a, T>(
    ptr: Option<&'a mut T>,
    log: &impl NullLog,
    func: &'static CStr,
    param: &'static CStr,
) -> Option<&'a mut T> {
    if ptr.is_none() {
        log.null_param(func, param);
    }
    ptr
}

/// `obs_service_get_protocol`.
pub fn protocol<S: Callbacks>(service: Option<&S>, log: &impl NullLog) -> Option<S::Str> {
    valid(service, log, c"obs_service_get_protocol").map(Callbacks::protocol)
}

/// `obs_service_get_connect_info`.
pub fn connect_info<S: Callbacks>(
    service: Option<&S>,
    log: &impl NullLog,
    kind: u32,
) -> Option<S::Str> {
    valid(service, log, c"obs_service_get_info")?.connect_info(kind)
}

/// `obs_service_can_try_to_connect`. No callback: true.
pub fn can_try_to_connect<S: Callbacks>(service: Option<&S>, log: &impl NullLog) -> bool {
    valid(service, log, c"obs_service_can_connect")
        .is_some_and(|s| s.can_try_to_connect().unwrap_or(true))
}

/// `obs_service_get_preferred_output_type`.
pub fn preferred_output_type<S: Callbacks>(
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<S::Str> {
    valid(service, log, c"obs_service_get_preferred_output_type")?.output_type()
}

/// `obs_service_get_max_fps`: checks the service, then `fps`, then zeroes
/// it before the callback.
pub fn max_fps<S: Callbacks>(service: Option<&S>, log: &impl NullLog, fps: Option<&mut i32>) {
    const FUNC: &CStr = c"obs_service_get_max_fps";
    let Some(service) = valid(service, log, FUNC) else {
        return;
    };
    let Some(fps) = valid_out(fps, log, FUNC, c"fps") else {
        return;
    };
    *fps = 0;
    service.max_fps(fps);
}

/// `obs_service_get_max_bitrate`: zeroes whichever out-parameters it got,
/// then checks the service (characterized, not endorsed).
pub fn max_bitrate<S: Callbacks>(
    service: Option<&S>,
    log: &impl NullLog,
    mut video: Option<&mut i32>,
    mut audio: Option<&mut i32>,
) {
    if let Some(video) = video.as_deref_mut() {
        *video = 0;
    }
    if let Some(audio) = audio.as_deref_mut() {
        *audio = 0;
    }
    if let Some(service) = valid(service, log, c"obs_service_get_max_bitrate") {
        service.max_bitrate(video, audio);
    }
}

/// `obs_service_get_supported_resolutions`: checks the service,
/// `resolutions` and `count` in that order, then writes `none` and 0 before
/// the callback. `none` is NULL in the C.
pub fn supported_resolutions<S: Callbacks>(
    service: Option<&S>,
    log: &impl NullLog,
    resolutions: Option<&mut S::Resolutions>,
    count: Option<&mut usize>,
    none: S::Resolutions,
) {
    const FUNC: &CStr = c"obs_service_supported_resolutions";
    let Some(service) = valid(service, log, FUNC) else {
        return;
    };
    let Some(resolutions) = valid_out(resolutions, log, FUNC, c"resolutions") else {
        return;
    };
    let Some(count) = valid_out(count, log, FUNC, c"count") else {
        return;
    };
    *resolutions = none;
    *count = 0;
    service.resolutions(resolutions, count);
}

/// `obs_service_get_supported_video_codecs`.
pub fn video_codecs<S: Callbacks>(service: &S) -> Option<S::List> {
    service.video_codecs()
}

/// `obs_service_get_supported_audio_codecs`.
pub fn audio_codecs<S: Callbacks>(service: &S) -> Option<S::List> {
    service.audio_codecs()
}
