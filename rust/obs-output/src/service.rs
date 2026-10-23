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

// ---------------------------------------------------------------------------
// Settings, properties and identity (#1913): `obs_service_get_display_name()`,
// `obs_service_defaults()`, `obs_get_service_properties()`,
// `obs_service_get_name()`, `_get_type()`, `_get_id()`, `_get_type_data()`,
// `_update()`, `_get_settings()`, `_properties()`,
// `_apply_encoder_settings()`. The signal and proc handler getters stay C:
// no public header declares them. The
// by-id functions get `find_service(id)` as `info`; the rest check the
// service with the C's function names, as the getters above do.

/// The `obs_data_t` / `obs_properties_t` calls these make, in libobs.
pub trait Host {
    /// `obs_data_t *`.
    type Data: Copy;
    /// `obs_properties_t *`, NULL included.
    type Props: Copy;

    fn data_create(&self) -> Self::Data;
    /// `obs_data_apply(target, apply)`; `apply` may be NULL.
    fn data_apply(&self, target: Self::Data, apply: Option<Self::Data>);
    fn data_addref(&self, data: Self::Data);
    fn data_release(&self, data: Self::Data);
    fn properties_apply_settings(&self, props: Self::Props, settings: Self::Data);
}

/// A registered service type (`struct obs_service_info`).
pub trait Type<H: Host> {
    /// `const char *`, passed through.
    type Str: Copy;
    /// `void *` plugin or type data, passed through; `Default` is NULL.
    type Ptr: Copy + Default;

    fn id(&self) -> Self::Str;
    fn type_data(&self) -> Self::Ptr;
    /// `get_name(type_data)`, required at registration.
    fn display_name(&self) -> Self::Str;
    /// Calls `get_defaults`; false when it is NULL.
    fn defaults(&self, settings: H::Data) -> bool;
    fn has_properties(&self) -> bool;
    /// `get_properties(data)`; only called when [`Type::has_properties`].
    fn properties(&self, data: Self::Ptr) -> H::Props;
    /// Calls `update`; false when it is NULL.
    fn update(&self, data: Self::Ptr, settings: H::Data) -> bool;
    fn has_apply_encoder_settings(&self) -> bool;
    /// `apply_encoder_settings(data, video, audio)`; only called when
    /// [`Type::has_apply_encoder_settings`].
    fn apply_encoder_settings(
        &self,
        data: Self::Ptr,
        video: Option<H::Data>,
        audio: Option<H::Data>,
    );
}

/// A live service: its type, plugin data and context fields.
pub trait Service<H: Host> {
    type Type: Type<H>;

    fn info(&self) -> &Self::Type;
    fn data(&self) -> <Self::Type as Type<H>>::Ptr;
    fn name(&self) -> <Self::Type as Type<H>>::Str;
    fn settings(&self) -> H::Data;
}

type StrOf<H, S> = <<S as Service<H>>::Type as Type<H>>::Str;
type PtrOf<H, S> = <<S as Service<H>>::Type as Type<H>>::Ptr;

/// `obs_service_get_display_name`.
pub fn display_name<H: Host, T: Type<H>>(info: Option<&T>) -> Option<T::Str> {
    info.map(Type::display_name)
}

/// `service_get_defaults`: a new object, filled by `get_defaults` if set.
fn get_defaults<H: Host, T: Type<H>>(host: &H, info: &T) -> H::Data {
    let settings = host.data_create();
    info.defaults(settings);
    settings
}

/// `obs_service_defaults`.
pub fn defaults<H: Host, T: Type<H>>(host: &H, info: Option<&T>) -> Option<H::Data> {
    info.map(|info| get_defaults(host, info))
}

/// `obs_get_service_properties`: defaults first, then `get_properties`
/// with NULL data, then the defaults applied and released.
pub fn type_properties<H: Host, T: Type<H>>(host: &H, info: Option<&T>) -> Option<H::Props> {
    let info = info.filter(|info| info.has_properties())?;
    let defaults = get_defaults(host, info);
    let props = info.properties(T::Ptr::default());
    host.properties_apply_settings(props, defaults);
    host.data_release(defaults);
    Some(props)
}

/// `obs_service_get_name`.
pub fn name<H: Host, S: Service<H>>(
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<StrOf<H, S>> {
    valid(service, log, c"obs_service_get_name").map(Service::name)
}

/// `obs_service_get_type`: the type's id.
pub fn type_id<H: Host, S: Service<H>>(
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<StrOf<H, S>> {
    valid(service, log, c"obs_service_get_type").map(|s| s.info().id())
}

/// `obs_service_get_id`: the same id, under its own log name.
pub fn id<H: Host, S: Service<H>>(service: Option<&S>, log: &impl NullLog) -> Option<StrOf<H, S>> {
    valid(service, log, c"obs_service_get_id").map(|s| s.info().id())
}

/// `obs_service_get_type_data`.
pub fn type_data<H: Host, S: Service<H>>(
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<PtrOf<H, S>> {
    valid(service, log, c"obs_service_get_type_data").map(|s| s.info().type_data())
}

/// `obs_service_properties`: `get_properties` with the plugin data, then the
/// service's settings applied.
pub fn properties<H: Host, S: Service<H>>(
    host: &H,
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<H::Props> {
    let service = valid(service, log, c"obs_service_properties")?;
    let info = service.info();
    if !info.has_properties() {
        return None;
    }
    let props = info.properties(service.data());
    host.properties_apply_settings(props, service.settings());
    Some(props)
}

/// `obs_service_update`: merges `settings` into the service's, then calls
/// `update` with the service's own settings.
pub fn update<H: Host, S: Service<H>>(
    host: &H,
    service: Option<&S>,
    log: &impl NullLog,
    settings: Option<H::Data>,
) {
    let Some(service) = valid(service, log, c"obs_service_update") else {
        return;
    };
    host.data_apply(service.settings(), settings);
    service.info().update(service.data(), service.settings());
}

/// `obs_service_get_settings`: the service's own object with a new ref.
pub fn settings<H: Host, S: Service<H>>(
    host: &H,
    service: Option<&S>,
    log: &impl NullLog,
) -> Option<H::Data> {
    let service = valid(service, log, c"obs_service_get_settings")?;
    host.data_addref(service.settings());
    Some(service.settings())
}

/// `obs_service_apply_encoder_settings`: not called when the callback is
/// NULL or both settings are.
pub fn apply_encoder_settings<H: Host, S: Service<H>>(
    service: Option<&S>,
    log: &impl NullLog,
    video: Option<H::Data>,
    audio: Option<H::Data>,
) {
    let Some(service) = valid(service, log, c"obs_service_apply_encoder_settings") else {
        return;
    };
    if !service.info().has_apply_encoder_settings() {
        return;
    }
    if video.is_some() || audio.is_some() {
        service
            .info()
            .apply_encoder_settings(service.data(), video, audio);
    }
}

// ---------------------------------------------------------------------------
// Lifecycle, references, module lookup and activation (#1920):
// `obs_service_create()`, `_create_private()`, `obs_service_destroy()`,
// `_release()`, `_get_ref()`, `_get_weak_service()`,
// `obs_weak_service_get_service()`, `_addref()`, `_release()`,
// `_references_service()`, `obs_service_get_module()`, `_load_state()` and
// the output-side `obs_service_activate()`, `_deactivate()` and
// `_initialize()`. `struct obs_service`, its context (`obs_context_data_*`)
// and the reference counts (`obs_ref_*`, `obs_weak_ref_*`) stay C; these
// functions keep the C's order of those calls, its log lines and its
// "destroy only once inactive" rule.

/// The libobs objects the lifecycle functions drive, as opaque handles.
pub trait Lifecycle {
    /// `obs_service_t *`, never NULL.
    type Svc: Copy + PartialEq;
    /// `obs_weak_service_t *`, never NULL.
    type Weak: Copy;
    /// `obs_output_t *`, never NULL.
    type Output: Copy;
    /// A registered `struct obs_service_info *`.
    type Info: Copy;
    /// `const char *`, passed through.
    type Str: Copy;
    /// `obs_data_t *`, NULL included, passed through.
    type Data: Copy;
    /// The plugin's data, never NULL.
    type Ptr: Copy;

    /// `find_service(id)`.
    fn find(&self, id: Self::Str) -> Option<Self::Info>;
    /// A zeroed service with `obs_context_data_init()` done; None when that
    /// fails (the C frees it).
    fn context_new(
        &self,
        name: Self::Str,
        settings: Self::Data,
        hotkey_data: Self::Data,
        private: bool,
    ) -> Option<Self::Svc>;
    /// Copies `info` into the service.
    fn set_info(&self, service: Self::Svc, info: Self::Info);
    /// The type's `create(settings, service)`, required at registration.
    fn call_create(&self, service: Self::Svc) -> Option<Self::Ptr>;
    fn set_plugin_data(&self, service: Self::Svc, data: Option<Self::Ptr>);
    fn plugin_data(&self, service: Self::Svc) -> Option<Self::Ptr>;
    /// `obs_context_init_control()` and `obs_context_data_insert()`.
    fn context_insert(&self, service: Self::Svc);
    fn context_remove(&self, service: Self::Svc);
    /// `obs_context_data_free()`, the owned id and the service itself.
    fn context_free(&self, service: Self::Svc);

    fn set_destroy(&self, service: Self::Svc);
    fn destroying(&self, service: Self::Svc) -> bool;
    fn active(&self, service: Self::Svc) -> bool;
    fn set_active(&self, service: Self::Svc, active: bool);
    fn has_output(&self, service: Self::Svc) -> bool;
    /// `service->output = NULL`.
    fn clear_output(&self, service: Self::Svc);
    /// `service->output->service = NULL`; only called with an output.
    fn detach_from_output(&self, service: Self::Svc);

    /// The type's `destroy(data)`, required at registration.
    fn call_destroy(&self, service: Self::Svc, data: Self::Ptr);
    /// Calls `activate(data, settings)`; false when it is NULL.
    fn call_activate(&self, service: Self::Svc) -> bool;
    /// Calls `deactivate(data)`; false when it is NULL.
    fn call_deactivate(&self, service: Self::Svc) -> bool;
    /// `initialize(data, output)`; None when it is NULL.
    fn call_initialize(&self, service: Self::Svc, output: Self::Output) -> Option<bool>;

    /// The service's weak control object.
    fn control(&self, service: Self::Svc) -> Self::Weak;
    /// `obs_ref_release(&control->ref)`: true for the last strong ref.
    fn ref_release(&self, weak: Self::Weak) -> bool;
    fn weak_addref(&self, weak: Self::Weak);
    /// `obs_weak_ref_release()`: true for the last weak ref.
    fn weak_release(&self, weak: Self::Weak) -> bool;
    /// `obs_weak_ref_get_ref()`: a new strong ref unless the service is gone.
    fn weak_get_ref(&self, weak: Self::Weak) -> bool;
    fn weak_target(&self, weak: Self::Weak) -> Self::Svc;
    fn weak_free(&self, weak: Self::Weak);

    /// `"Service '%s' not found"`, LOG_ERROR.
    fn log_not_found(&self, id: Self::Str);
    /// `"Failed to create service '%s'!"`, LOG_ERROR.
    fn log_create_failed(&self, name: Self::Str);
    /// `"service '%s' (%s) created"`, LOG_DEBUG.
    fn log_created(&self, name: Self::Str, id: Self::Str);
    /// `"service '%s' destroyed"` with the context name, LOG_DEBUG.
    fn log_destroyed(&self, service: Self::Svc);
    /// `"obs_service_deactivate: service '%s' is not assigned to an
    /// output"` with the service's name, LOG_WARNING; activation logs it
    /// under the same name (characterized, not endorsed).
    fn log_no_output(&self, service: Self::Svc);
}

/// `obs_service_valid(service, func)` for a handle.
fn valid_handle<S: Copy>(service: Option<S>, log: &impl NullLog, func: &'static CStr) -> Option<S> {
    if service.is_none() {
        log.null_param(func, c"service");
    }
    service
}

/// `obs_service_create_internal`: a type that is not registered logs and
/// gives None; a `create()` that returns NULL logs and still gives the
/// service.
pub fn create<L: Lifecycle>(
    o: &L,
    id: L::Str,
    name: L::Str,
    settings: L::Data,
    hotkey_data: L::Data,
    private: bool,
) -> Option<L::Svc> {
    let Some(info) = o.find(id) else {
        o.log_not_found(id);
        return None;
    };
    let service = o.context_new(name, settings, hotkey_data, private)?;
    o.set_info(service, info);
    let data = o.call_create(service);
    o.set_plugin_data(service, data);
    if data.is_none() {
        o.log_create_failed(name);
    }
    o.context_insert(service);
    o.log_created(name, id);
    Some(service)
}

/// `actually_destroy_service`.
fn actually_destroy<L: Lifecycle>(o: &L, service: L::Svc) {
    if let Some(data) = o.plugin_data(service) {
        o.call_destroy(service, data);
    }
    if o.has_output(service) {
        o.detach_from_output(service);
    }
    o.log_destroyed(service);
    o.context_free(service);
}

/// `obs_service_destroy`: unlisted at once, freed only once inactive.
pub fn destroy<L: Lifecycle>(o: &L, service: Option<L::Svc>) {
    let Some(service) = service else {
        return;
    };
    o.context_remove(service);
    o.set_destroy(service);
    if !o.active(service) {
        actually_destroy(o, service);
    }
}

/// `obs_service_activate`.
pub fn activate<L: Lifecycle>(o: &L, log: &impl NullLog, service: Option<L::Svc>) {
    let Some(service) = valid_handle(service, log, c"obs_service_activate") else {
        return;
    };
    if !o.has_output(service) {
        o.log_no_output(service);
        return;
    }
    if o.active(service) {
        return;
    }
    o.call_activate(service);
    o.set_active(service, true);
}

/// `obs_service_deactivate`: a destroyed service is freed here; otherwise
/// `remove` detaches it from its output.
pub fn deactivate<L: Lifecycle>(o: &L, log: &impl NullLog, service: Option<L::Svc>, remove: bool) {
    let Some(service) = valid_handle(service, log, c"obs_service_deactivate") else {
        return;
    };
    if !o.has_output(service) {
        o.log_no_output(service);
        return;
    }
    if !o.active(service) {
        return;
    }
    o.call_deactivate(service);
    o.set_active(service, false);
    if o.destroying(service) {
        actually_destroy(o, service);
    } else if remove {
        o.clear_output(service);
    }
}

/// `obs_service_initialize`: no callback is success.
pub fn initialize<L: Lifecycle>(
    o: &L,
    log: &impl NullLog,
    service: Option<L::Svc>,
    output: Option<L::Output>,
) -> bool {
    const FUNC: &CStr = c"obs_service_initialize";
    let Some(service) = valid_handle(service, log, FUNC) else {
        return false;
    };
    let Some(output) = output else {
        log.null_param(FUNC, c"output");
        return false;
    };
    o.call_initialize(service, output).unwrap_or(true)
}

/// `obs_service_release`: the last strong ref destroys the service, then
/// drops the control's own weak ref (in that order: the context list relies
/// on the weak ref while the service is listed).
pub fn release<L: Lifecycle>(o: &L, service: Option<L::Svc>) {
    let Some(service) = service else {
        return;
    };
    let control = o.control(service);
    if o.ref_release(control) {
        destroy(o, Some(service));
        weak_release(o, Some(control));
    }
}

/// `obs_weak_service_addref`.
pub fn weak_addref<L: Lifecycle>(o: &L, weak: Option<L::Weak>) {
    if let Some(weak) = weak {
        o.weak_addref(weak);
    }
}

/// `obs_weak_service_release`: the last weak ref frees the control.
pub fn weak_release<L: Lifecycle>(o: &L, weak: Option<L::Weak>) {
    if let Some(weak) = weak
        && o.weak_release(weak)
    {
        o.weak_free(weak);
    }
}

/// `obs_service_get_ref`.
pub fn get_ref<L: Lifecycle>(o: &L, service: Option<L::Svc>) -> Option<L::Svc> {
    weak_get_service(o, Some(o.control(service?)))
}

/// `obs_service_get_weak_service`: the control, with a new weak ref.
pub fn get_weak_service<L: Lifecycle>(o: &L, service: Option<L::Svc>) -> Option<L::Weak> {
    let weak = o.control(service?);
    weak_addref(o, Some(weak));
    Some(weak)
}

/// `obs_weak_service_get_service`: a new strong ref, or None once gone.
pub fn weak_get_service<L: Lifecycle>(o: &L, weak: Option<L::Weak>) -> Option<L::Svc> {
    let weak = weak?;
    o.weak_get_ref(weak).then(|| o.weak_target(weak))
}

/// `obs_weak_service_references_service`.
pub fn weak_references<L: Lifecycle>(
    o: &L,
    weak: Option<L::Weak>,
    service: Option<L::Svc>,
) -> bool {
    match (weak, service) {
        (Some(weak), Some(service)) => o.weak_target(weak) == service,
        _ => false,
    }
}

/// The loaded and the disabled module lists, for the by-id lookups.
pub trait Modules {
    /// `obs_module_t *`, never NULL.
    type Module: Copy;
    /// `const char *`, passed through.
    type Str: Copy;

    /// `obs->first_module`, or `obs->first_disabled_module`.
    fn first(&self, disabled: bool) -> Option<Self::Module>;
    fn next(&self, module: Self::Module) -> Option<Self::Module>;
    fn service_count(&self, module: Self::Module) -> usize;
    /// `strcmp(module->services.array[i], id) == 0`.
    fn service_is(&self, module: Self::Module, i: usize, id: Self::Str) -> bool;
    /// `module->load_state`, an `enum obs_module_load_state`.
    fn load_state(&self, module: Self::Module) -> i32;
}

/// `OBS_MODULE_MISSING` (`obs.h`).
pub const MODULE_MISSING: i32 = 2;

/// `obs_service_get_module`: the first loaded, then the first disabled
/// module that registered `id`.
pub fn module<M: Modules>(m: &M, id: M::Str) -> Option<M::Module> {
    for disabled in [false, true] {
        let mut cur = m.first(disabled);
        while let Some(module) = cur {
            if (0..m.service_count(module)).any(|i| m.service_is(module, i, id)) {
                return Some(module);
            }
            cur = m.next(module);
        }
    }
    None
}

/// `obs_service_load_state`.
pub fn load_state<M: Modules>(m: &M, id: M::Str) -> i32 {
    module(m, id).map_or(MODULE_MISSING, |module| m.load_state(module))
}
