//! C ABI for `libobs/obs-service-getters.c.h` (#1899): the nine service
//! capability and connect-info getters, exported under their C names
//! (`libobs/cmake/rust-exports.d/obs-output-service.txt`).
//!
//! `struct obs_service` stays private to C. A shim reaches the plugin's
//! callback table through the hidden `obs_service_get_info_table()` that
//! `obs-service.c.h` keeps next to the struct layout, and the plugin's data
//! through the hidden `obs_service_get_plugin_data()` beside it. [`ObsServiceInfo`] mirrors the
//! public plugin ABI `struct obs_service_info` (`obs-service.h`) field for
//! field; only the callbacks the getters call are read.
//!
//! Every check, write and call happens in the C's order, through
//! [`crate::service`]; the NULL cases are the C's, including the codec
//! getters, which dereference a NULL service as the C does.

use core::ffi::{CStr, c_char, c_int, c_void};
use core::ptr;

use crate::service::{self, Callbacks, Host, NullLog, Service, Type};

/// `struct obs_service_resolution` (`obs-service.h`).
#[repr(C)]
pub struct ObsServiceResolution {
    pub cx: c_int,
    pub cy: c_int,
}

type DataFn<R> = Option<unsafe extern "C" fn(data: *mut c_void) -> R>;

/// `struct obs_service_info` (`obs-service.h`), the public plugin ABI.
#[repr(C)]
pub struct ObsServiceInfo {
    pub id: *const c_char,
    pub get_name: Option<unsafe extern "C" fn(type_data: *mut c_void) -> *const c_char>,
    pub create:
        Option<unsafe extern "C" fn(settings: *mut c_void, service: *mut c_void) -> *mut c_void>,
    pub destroy: DataFn<()>,
    pub activate: Option<unsafe extern "C" fn(data: *mut c_void, settings: *mut c_void)>,
    pub deactivate: DataFn<()>,
    pub update: Option<unsafe extern "C" fn(data: *mut c_void, settings: *mut c_void)>,
    pub get_defaults: Option<unsafe extern "C" fn(settings: *mut c_void)>,
    pub get_properties: DataFn<*mut c_void>,
    pub initialize: Option<unsafe extern "C" fn(data: *mut c_void, output: *mut c_void) -> bool>,
    pub get_url: DataFn<*const c_char>,
    pub get_key: DataFn<*const c_char>,
    pub get_username: DataFn<*const c_char>,
    pub get_password: DataFn<*const c_char>,
    pub deprecated_1: Option<unsafe extern "C" fn() -> bool>,
    pub apply_encoder_settings:
        Option<unsafe extern "C" fn(data: *mut c_void, video: *mut c_void, audio: *mut c_void)>,
    pub type_data: *mut c_void,
    pub free_type_data: Option<unsafe extern "C" fn(type_data: *mut c_void)>,
    pub get_output_type: DataFn<*const c_char>,
    pub get_supported_resolutions: Option<
        unsafe extern "C" fn(
            data: *mut c_void,
            resolutions: *mut *mut ObsServiceResolution,
            count: *mut usize,
        ),
    >,
    pub get_max_fps: Option<unsafe extern "C" fn(data: *mut c_void, fps: *mut c_int)>,
    pub get_max_bitrate:
        Option<unsafe extern "C" fn(data: *mut c_void, video: *mut c_int, audio: *mut c_int)>,
    pub get_supported_video_codecs: DataFn<*mut *const c_char>,
    pub get_protocol: DataFn<*const c_char>,
    pub get_supported_audio_codecs: DataFn<*mut *const c_char>,
    pub get_connect_info:
        Option<unsafe extern "C" fn(data: *mut c_void, kind: u32) -> *const c_char>,
    pub can_try_to_connect: DataFn<bool>,
}

/// `LOG_DEBUG` (`util/base.h`).
const LOG_DEBUG: c_int = 400;

unsafe extern "C" {
    fn blog(level: c_int, format: *const c_char, ...);
    fn obs_service_get_plugin_data(service: *const c_void) -> *mut c_void;
    fn obs_service_get_info_table(service: *const c_void) -> *const ObsServiceInfo;
    fn obs_service_get_context_name(service: *const c_void) -> *const c_char;
    fn obs_service_get_context_settings(service: *const c_void) -> *mut c_void;
    fn find_service(id: *const c_char) -> *const ObsServiceInfo;
    fn obs_data_create() -> *mut c_void;
    fn obs_data_apply(target: *mut c_void, apply_data: *mut c_void);
    fn obs_data_addref(data: *mut c_void);
    fn obs_data_release(data: *mut c_void);
    fn obs_properties_apply_settings(props: *mut c_void, settings: *mut c_void);
}

/// A live service: its callback table and its plugin data.
struct CService {
    info: *const ObsServiceInfo,
    data: *mut c_void,
}

impl CService {
    /// # Safety
    ///
    /// `service` is a live, non-NULL `obs_service_t`.
    unsafe fn new(service: *const c_void) -> Self {
        // SAFETY: a live service (caller contract).
        unsafe {
            CService {
                info: obs_service_get_info_table(service),
                data: obs_service_get_plugin_data(service),
            }
        }
    }

    fn info(&self) -> &ObsServiceInfo {
        // SAFETY: the table lives as long as the service.
        unsafe { &*self.info }
    }
}

// SAFETY, every call below: a plugin callback of this service's table,
// with the service's own data, as the C calls it.
impl Callbacks for CService {
    type Str = *const c_char;
    type List = *mut *const c_char;
    type Resolutions = *mut ObsServiceResolution;

    fn protocol(&self) -> *const c_char {
        // Required at registration; the C calls it without a check.
        let f = self
            .info()
            .get_protocol
            .expect("obs_register_service requires get_protocol");
        unsafe { f(self.data) }
    }
    fn connect_info(&self, kind: u32) -> Option<*const c_char> {
        self.info()
            .get_connect_info
            .map(|f| unsafe { f(self.data, kind) })
    }
    fn can_try_to_connect(&self) -> Option<bool> {
        self.info()
            .can_try_to_connect
            .map(|f| unsafe { f(self.data) })
    }
    fn output_type(&self) -> Option<*const c_char> {
        self.info().get_output_type.map(|f| unsafe { f(self.data) })
    }
    fn max_fps(&self, fps: &mut i32) -> bool {
        self.info()
            .get_max_fps
            .map(|f| unsafe { f(self.data, fps) })
            .is_some()
    }
    fn max_bitrate(&self, video: Option<&mut i32>, audio: Option<&mut i32>) -> bool {
        let video = video.map_or(ptr::null_mut(), ptr::from_mut);
        let audio = audio.map_or(ptr::null_mut(), ptr::from_mut);
        self.info()
            .get_max_bitrate
            .map(|f| unsafe { f(self.data, video, audio) })
            .is_some()
    }
    fn resolutions(&self, resolutions: &mut Self::Resolutions, count: &mut usize) -> bool {
        self.info()
            .get_supported_resolutions
            .map(|f| unsafe { f(self.data, resolutions, count) })
            .is_some()
    }
    fn video_codecs(&self) -> Option<Self::List> {
        self.info()
            .get_supported_video_codecs
            .map(|f| unsafe { f(self.data) })
    }
    fn audio_codecs(&self) -> Option<Self::List> {
        self.info()
            .get_supported_audio_codecs
            .map(|f| unsafe { f(self.data) })
    }
}

/// `obs_ptr_valid`'s debug line.
struct CLog;

impl NullLog for CLog {
    fn null_param(&self, func: &'static CStr, param: &'static CStr) {
        // SAFETY: the format and its two string arguments.
        unsafe {
            blog(
                LOG_DEBUG,
                c"%s: Null '%s' parameter".as_ptr(),
                func.as_ptr(),
                param.as_ptr(),
            )
        };
    }
}

/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
unsafe fn wrap(service: *const c_void) -> Option<CService> {
    // SAFETY: live when not NULL (caller contract).
    (!service.is_null()).then(|| unsafe { CService::new(service) })
}

/// Same contract as C `obs_service_get_protocol`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_protocol(service: *const c_void) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { wrap(service) };
    service::protocol(s.as_ref(), &CLog).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_get_connect_info`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_connect_info(
    service: *const c_void,
    kind: u32,
) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { wrap(service) };
    service::connect_info(s.as_ref(), &CLog, kind).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_can_try_to_connect`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_can_try_to_connect(service: *const c_void) -> bool {
    // SAFETY: forwarded caller contract.
    let s = unsafe { wrap(service) };
    service::can_try_to_connect(s.as_ref(), &CLog)
}

/// Same contract as C `obs_service_get_preferred_output_type`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_preferred_output_type(
    service: *const c_void,
) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { wrap(service) };
    service::preferred_output_type(s.as_ref(), &CLog).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_get_max_fps`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`; `fps` is NULL or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_max_fps(service: *const c_void, fps: *mut c_int) {
    // SAFETY: forwarded caller contract.
    let (s, fps) = unsafe { (wrap(service), fps.as_mut()) };
    service::max_fps(s.as_ref(), &CLog, fps);
}

/// Same contract as C `obs_service_get_max_bitrate`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`; each out-parameter is NULL
/// or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_max_bitrate(
    service: *const c_void,
    video_bitrate: *mut c_int,
    audio_bitrate: *mut c_int,
) {
    // SAFETY: forwarded caller contract.
    let (s, video, audio) = unsafe {
        (
            wrap(service),
            video_bitrate.as_mut(),
            audio_bitrate.as_mut(),
        )
    };
    service::max_bitrate(s.as_ref(), &CLog, video, audio);
}

/// Same contract as C `obs_service_get_supported_resolutions`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`; each out-parameter is NULL
/// or writable.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_supported_resolutions(
    service: *const c_void,
    resolutions: *mut *mut ObsServiceResolution,
    count: *mut usize,
) {
    // SAFETY: forwarded caller contract.
    let (s, resolutions, count) = unsafe { (wrap(service), resolutions.as_mut(), count.as_mut()) };
    service::supported_resolutions(s.as_ref(), &CLog, resolutions, count, ptr::null_mut());
}

/// Same contract as C `obs_service_get_supported_video_codecs`: the C
/// dereferences `service` without a check, and so does this.
///
/// # Safety
///
/// `service` is a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_supported_video_codecs(
    service: *const c_void,
) -> *mut *const c_char {
    // SAFETY: live (caller contract).
    let s = unsafe { CService::new(service) };
    service::video_codecs(&s).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_service_get_supported_audio_codecs`.
///
/// # Safety
///
/// `service` is a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_supported_audio_codecs(
    service: *const c_void,
) -> *mut *const c_char {
    // SAFETY: live (caller contract).
    let s = unsafe { CService::new(service) };
    service::audio_codecs(&s).unwrap_or(ptr::null_mut())
}

// ---------------------------------------------------------------------------
// Settings, properties and identity (#1913). The context fields come through
// the hidden obs_service_get_context_*() accessors, the type of an id
// through find_service(), and the obs_data_t / obs_properties_t calls go to
// libobs.

/// libobs' `obs_data_*` and `obs_properties_apply_settings`.
struct CHost;

// SAFETY, every call below: libobs functions with the pointers the C
// passes them, as the C does.
impl Host for CHost {
    type Data = *mut c_void;
    type Props = *mut c_void;

    fn data_create(&self) -> *mut c_void {
        unsafe { obs_data_create() }
    }
    fn data_apply(&self, target: *mut c_void, apply: Option<*mut c_void>) {
        unsafe { obs_data_apply(target, apply.unwrap_or(ptr::null_mut())) }
    }
    fn data_addref(&self, data: *mut c_void) {
        unsafe { obs_data_addref(data) }
    }
    fn data_release(&self, data: *mut c_void) {
        unsafe { obs_data_release(data) }
    }
    fn properties_apply_settings(&self, props: *mut c_void, settings: *mut c_void) {
        unsafe { obs_properties_apply_settings(props, settings) }
    }
}

/// A registered table, as the type of the functions of #1913 (private, so
/// its callers stay behind the `unsafe` shims).
#[repr(transparent)]
struct CType(ObsServiceInfo);

impl CType {
    fn of(info: &ObsServiceInfo) -> &CType {
        // SAFETY: repr(transparent) over ObsServiceInfo.
        unsafe { &*ptr::from_ref(info).cast::<CType>() }
    }
}

impl core::ops::Deref for CType {
    type Target = ObsServiceInfo;
    fn deref(&self) -> &ObsServiceInfo {
        &self.0
    }
}

// SAFETY, every call below: a plugin callback of this registered table, as
// the C calls it.
impl Type<CHost> for CType {
    type Str = *const c_char;
    type Ptr = *mut c_void;

    fn id(&self) -> *const c_char {
        self.id
    }
    fn type_data(&self) -> *mut c_void {
        self.type_data
    }
    fn display_name(&self) -> *const c_char {
        let f = self
            .get_name
            .expect("obs_register_service requires get_name");
        unsafe { f(self.type_data) }
    }
    fn defaults(&self, settings: *mut c_void) -> bool {
        self.get_defaults.map(|f| unsafe { f(settings) }).is_some()
    }
    fn has_properties(&self) -> bool {
        self.get_properties.is_some()
    }
    fn properties(&self, data: *mut c_void) -> *mut c_void {
        self.get_properties
            .map_or(ptr::null_mut(), |f| unsafe { f(data) })
    }
    fn update(&self, data: *mut c_void, settings: *mut c_void) -> bool {
        self.update.map(|f| unsafe { f(data, settings) }).is_some()
    }
    fn has_apply_encoder_settings(&self) -> bool {
        self.apply_encoder_settings.is_some()
    }
    fn apply_encoder_settings(
        &self,
        data: *mut c_void,
        video: Option<*mut c_void>,
        audio: Option<*mut c_void>,
    ) {
        if let Some(f) = self.apply_encoder_settings {
            unsafe {
                f(
                    data,
                    video.unwrap_or(ptr::null_mut()),
                    audio.unwrap_or(ptr::null_mut()),
                )
            }
        }
    }
}

/// A live service with its context, for the functions of #1913.
struct CContext {
    service: *const c_void,
    inner: CService,
}

impl CContext {
    /// # Safety
    ///
    /// `service` is NULL or a live `obs_service_t`.
    unsafe fn wrap(service: *const c_void) -> Option<Self> {
        // SAFETY: live when not NULL (caller contract).
        (!service.is_null()).then(|| CContext {
            service,
            inner: unsafe { CService::new(service) },
        })
    }
}

// SAFETY, every call below: a hidden accessor of a live service.
impl Service<CHost> for CContext {
    type Type = CType;

    fn info(&self) -> &CType {
        CType::of(self.inner.info())
    }
    fn data(&self) -> *mut c_void {
        self.inner.data
    }
    fn name(&self) -> *const c_char {
        unsafe { obs_service_get_context_name(self.service) }
    }
    fn settings(&self) -> *mut c_void {
        unsafe { obs_service_get_context_settings(self.service) }
    }
}

/// `find_service(id)`.
///
/// # Safety
///
/// `id` is a C string; libobs is started.
unsafe fn find(id: *const c_char) -> Option<&'static CType> {
    // SAFETY: the registered table lives until obs_shutdown.
    unsafe { find_service(id).as_ref() }.map(CType::of)
}

fn nullable(data: *mut c_void) -> Option<*mut c_void> {
    (!data.is_null()).then_some(data)
}

/// Same contract as C `obs_service_get_display_name`.
///
/// # Safety
///
/// `id` is a C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_display_name(id: *const c_char) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let info = unsafe { find(id) };
    service::display_name::<CHost, _>(info).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_defaults`.
///
/// # Safety
///
/// `id` is a C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_defaults(id: *const c_char) -> *mut c_void {
    // SAFETY: forwarded caller contract.
    let info = unsafe { find(id) };
    service::defaults(&CHost, info).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_get_service_properties`.
///
/// # Safety
///
/// `id` is a C string.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_get_service_properties(id: *const c_char) -> *mut c_void {
    // SAFETY: forwarded caller contract.
    let info = unsafe { find(id) };
    service::type_properties(&CHost, info).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_service_get_name`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_name(service: *const c_void) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::name::<CHost, _>(s.as_ref(), &CLog).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_get_type`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_type(service: *const c_void) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::type_id::<CHost, _>(s.as_ref(), &CLog).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_get_id`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_id(service: *const c_void) -> *const c_char {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::id::<CHost, _>(s.as_ref(), &CLog).unwrap_or(ptr::null())
}

/// Same contract as C `obs_service_get_type_data`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_type_data(service: *mut c_void) -> *mut c_void {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::type_data::<CHost, _>(s.as_ref(), &CLog).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_service_properties`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_properties(service: *const c_void) -> *mut c_void {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::properties(&CHost, s.as_ref(), &CLog).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_service_update`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`; `settings` is NULL or a
/// live `obs_data_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_update(service: *mut c_void, settings: *mut c_void) {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::update(&CHost, s.as_ref(), &CLog, nullable(settings));
}

/// Same contract as C `obs_service_get_settings`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_get_settings(service: *const c_void) -> *mut c_void {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::settings(&CHost, s.as_ref(), &CLog).unwrap_or(ptr::null_mut())
}

/// Same contract as C `obs_service_apply_encoder_settings`.
///
/// # Safety
///
/// `service` is NULL or a live `obs_service_t`; each settings object is
/// NULL or a live `obs_data_t`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_service_apply_encoder_settings(
    service: *mut c_void,
    video_encoder_settings: *mut c_void,
    audio_encoder_settings: *mut c_void,
) {
    // SAFETY: forwarded caller contract.
    let s = unsafe { CContext::wrap(service) };
    service::apply_encoder_settings::<CHost, _>(
        s.as_ref(),
        &CLog,
        nullable(video_encoder_settings),
        nullable(audio_encoder_settings),
    );
}
