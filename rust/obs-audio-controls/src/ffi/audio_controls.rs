//! C ABI for `libobs/obs-audio-controls.c.h`.
//!
//! `obs_fader_t` and `obs_volmeter_t` are opaque. The handle is one
//! `bzalloc` of a pointer, which is the allocation `bnum_allocs` counts.
//! A bad fader type frees that block. A volmeter keeps it for any type.
//! The [`crate::Fader`] and [`crate::Volmeter`] live in a `Box`. Callback
//! slots do too, so they are not counted.
//!
//! The safe core does not call obs. This module copies a source pointer,
//! connects `volume` and `destroy`, and for a meter the capture callback.
//! `set_db` (and deflection and mul, which go through it) calls
//! `obs_source_set_volume` once when a source is attached. That signal is
//! what runs [`crate::Fader::on_volume`]. This module does not call
//! `on_volume` itself.
//!
//! `obs_source_get_signal_handler`, the volume and mute and capture calls,
//! `obs_source_get_speaker_layout`, `obs_get_audio_info`, and
//! `signal_handler_connect` / `disconnect` stay in `obs-source.c.h` and
//! `callback/signal.c`. Cargo tests supply them. libobs supplies them for
//! the real library. `blog` is `util/base-variadic.c`. `bmalloc` is the
//! obs-util port.

use std::ffi::{c_char, c_int, c_void};
use std::ptr;
use std::sync::Mutex;

use obs_media_io::ffi::audio_io::audio_data;
use obs_util::ffi::bmem::{bfree, bmalloc};

use crate::curves::{self, Curve};
use crate::volmeter::AudioPacket;
use crate::{
    FADER_IEC, FADER_LOG, Fader, FaderCb, MAX_AUDIO_CHANNELS, MeterCb, Volmeter, db_to_mul,
    mul_to_db,
};

/// `LOG_DEBUG` in `util/base.h`. It is an enum, not a macro.
const LOG_DEBUG: c_int = 400;

/// Mirrors `struct obs_audio_info` (`uint32_t`, then `enum speaker_layout`).
#[repr(C)]
pub struct ObsAudioInfo {
    pub samples_per_sec: u32,
    pub speakers: i32,
}

type FaderFn = Option<unsafe extern "C" fn(*mut c_void, f32)>;
type MeterFn = Option<unsafe extern "C" fn(*mut c_void, *const f32, *const f32, *const f32)>;
type SignalCb = unsafe extern "C" fn(*mut c_void, *mut c_void);
type CaptureCb = unsafe extern "C" fn(*mut c_void, *mut c_void, *const audio_data, bool);

unsafe extern "C" {
    fn blog(level: c_int, fmt: *const c_char, ...);
    fn printf(fmt: *const c_char, ...) -> c_int;
    fn calldata_get_data(
        data: *const c_void,
        name: *const c_char,
        out: *mut c_void,
        size: usize,
    ) -> bool;
    fn obs_source_get_signal_handler(source: *mut c_void) -> *mut c_void;
    fn obs_source_get_volume(source: *mut c_void) -> f32;
    fn obs_source_set_volume(source: *mut c_void, volume: f32);
    fn obs_source_muted(source: *mut c_void) -> bool;
    fn obs_source_get_speaker_layout(source: *mut c_void) -> c_int;
    fn obs_source_add_audio_capture_callback(
        source: *mut c_void,
        callback: CaptureCb,
        param: *mut c_void,
    );
    fn obs_source_remove_audio_capture_callback(
        source: *mut c_void,
        callback: CaptureCb,
        param: *mut c_void,
    );
    fn obs_get_audio_info(info: *mut ObsAudioInfo) -> bool;
    fn signal_handler_connect(
        handler: *mut c_void,
        signal: *const c_char,
        callback: SignalCb,
        data: *mut c_void,
    );
    fn signal_handler_disconnect(
        handler: *mut c_void,
        signal: *const c_char,
        callback: SignalCb,
        data: *mut c_void,
    );
}

struct FaderSlot {
    callback: FaderFn,
    param: usize,
}

struct MeterSlot {
    callback: MeterFn,
    param: usize,
}

/// Source pointer bits live in the mutex. `*mut c_void` is not `Send`.
/// Null is 0. The guard is dropped before any core call and before
/// `signal_handler_connect`, `obs_source_set_volume`, or a disconnect.
struct FaderHost {
    fader: Fader,
    source: Mutex<usize>,
    /// The `Box` keeps the slot address stable when the `Vec` grows.
    /// The safe core holds that address.
    #[allow(clippy::vec_box)]
    slots: Mutex<Vec<Box<FaderSlot>>>,
}

struct MeterHost {
    meter: Volmeter,
    source: Mutex<usize>,
    /// The `Box` keeps the slot address stable when the `Vec` grows.
    /// The safe core holds that address.
    #[allow(clippy::vec_box)]
    slots: Mutex<Vec<Box<MeterSlot>>>,
}

/// `bzalloc`: `bmalloc` plus a zero of the block. A null result is returned.
unsafe fn bzalloc(size: usize) -> *mut c_void {
    // SAFETY: one pointer-sized block. Size is never 0.
    let mem = unsafe { bmalloc(size) };
    if !mem.is_null() {
        // SAFETY: `bmalloc` returned `size` bytes.
        unsafe { ptr::write_bytes(mem.cast::<u8>(), 0, size) };
    }
    mem
}

/// `obs_ptr_valid`: null logs `blog(LOG_DEBUG, "%s: Null '%s' parameter", ...)`.
unsafe fn ptr_valid(ptr: *mut c_void, func: &std::ffi::CStr, name: &std::ffi::CStr) -> bool {
    if ptr.is_null() {
        // SAFETY: both strings are literals. `blog` is the C variadic.
        unsafe {
            blog(
                LOG_DEBUG,
                c"%s: Null '%s' parameter".as_ptr(),
                func.as_ptr(),
                name.as_ptr(),
            );
        }
        false
    } else {
        true
    }
}

fn same_fader_fn(a: FaderFn, b: FaderFn) -> bool {
    match (a, b) {
        (Some(a), Some(b)) => std::ptr::fn_addr_eq(a, b),
        (None, None) => true,
        _ => false,
    }
}

fn same_meter_fn(a: MeterFn, b: MeterFn) -> bool {
    match (a, b) {
        (Some(a), Some(b)) => std::ptr::fn_addr_eq(a, b),
        (None, None) => true,
        _ => false,
    }
}

/// # Safety
/// `handle` is a live fader from [`obs_fader_create`].
unsafe fn fader_host<'a>(handle: *mut c_void) -> &'a FaderHost {
    // SAFETY: the block holds the `Box` pointer written at create.
    let host = unsafe { handle.cast::<*mut FaderHost>().read() };
    // SAFETY: that pointer is the live host until destroy.
    unsafe { &*host }
}

/// # Safety
/// `handle` is a live meter from [`obs_volmeter_create`].
unsafe fn meter_host<'a>(handle: *mut c_void) -> &'a MeterHost {
    // SAFETY: the block holds the `Box` pointer written at create.
    let host = unsafe { handle.cast::<*mut MeterHost>().read() };
    // SAFETY: that pointer is the live host until destroy.
    unsafe { &*host }
}

fn host_data<T>(host: &T) -> *mut c_void {
    ptr::from_ref(host).cast_mut().cast::<c_void>()
}

fn fader_trampoline(param: usize, db: f32) {
    // SAFETY: `param` is the slot address registered with this trampoline.
    // The slot stays allocated until `remove_callback` drops it, and that
    // runs only after the safe core has dropped this registration.
    let slot = unsafe { &*(param as *const FaderSlot) };
    if let Some(callback) = slot.callback {
        // SAFETY: the callback is the C function the caller registered.
        unsafe { callback(slot.param as *mut c_void, db) };
    }
}

fn meter_trampoline(
    param: usize,
    magnitude: &[f32; MAX_AUDIO_CHANNELS],
    peak: &[f32; MAX_AUDIO_CHANNELS],
    input_peak: &[f32; MAX_AUDIO_CHANNELS],
) {
    // SAFETY: `param` is the slot address registered with this trampoline.
    let slot = unsafe { &*(param as *const MeterSlot) };
    if let Some(callback) = slot.callback {
        // SAFETY: the three arrays are the eight-channel buffers the safe
        // core just filled. The callback is the one the caller registered.
        unsafe {
            callback(
                slot.param as *mut c_void,
                magnitude.as_ptr(),
                peak.as_ptr(),
                input_peak.as_ptr(),
            );
        }
    }
}

/// `calldata_float`: the header inline over `calldata_get_data`. The bool
/// is ignored and a missing value is 0, then cast to float.
unsafe fn calldata_volume(calldata: *mut c_void) -> f32 {
    let mut value = 0.0f64;
    // SAFETY: `calldata` is the signal's calldata. `value` is one double.
    unsafe {
        calldata_get_data(
            calldata,
            c"volume".as_ptr(),
            ptr::from_mut(&mut value).cast::<c_void>(),
            size_of::<f64>(),
        );
    }
    value as f32
}

extern "C" fn cubic_db_to_def(db: f32) -> f32 {
    curves::db_to_def(Curve::Cubic, db)
}

extern "C" fn iec_db_to_def(db: f32) -> f32 {
    curves::db_to_def(Curve::Iec, db)
}

extern "C" fn log_db_to_def(db: f32) -> f32 {
    curves::db_to_def(Curve::Log, db)
}

fn push_volume(host: &FaderHost) {
    let source = *host.source.lock().expect("fader source");
    if source == 0 {
        return;
    }
    let Some(mul) = host.fader.attached_volume() else {
        return;
    };
    // SAFETY: `source` is the pointer `attach` stored. The guard is dropped,
    // so a volume signal that detaches can take it.
    unsafe { obs_source_set_volume(source as *mut c_void, mul) };
}

fn detach_fader(host: &FaderHost) {
    let source = {
        let mut guard = host.source.lock().expect("fader source");
        let source = *guard;
        *guard = 0;
        source
    };
    host.fader.detach();
    if source == 0 {
        return;
    }
    let source = source as *mut c_void;
    let data = host_data(host);
    // SAFETY: `source` was attached. Disconnect uses the same callbacks and
    // the same host pointer `attach` connected.
    unsafe {
        let handler = obs_source_get_signal_handler(source);
        signal_handler_disconnect(handler, c"volume".as_ptr(), fader_on_volume, data);
        signal_handler_disconnect(handler, c"destroy".as_ptr(), fader_on_destroy, data);
    }
}

fn detach_meter(host: &MeterHost) {
    let source = {
        let mut guard = host.source.lock().expect("meter source");
        let source = *guard;
        *guard = 0;
        source
    };
    host.meter.detach();
    if source == 0 {
        return;
    }
    let source = source as *mut c_void;
    let data = host_data(host);
    // SAFETY: `source` was attached. Disconnect uses the same callbacks and
    // the same host pointer `attach` connected, including the capture one.
    unsafe {
        let handler = obs_source_get_signal_handler(source);
        signal_handler_disconnect(handler, c"volume".as_ptr(), meter_on_volume, data);
        signal_handler_disconnect(handler, c"destroy".as_ptr(), meter_on_destroy, data);
        obs_source_remove_audio_capture_callback(source, meter_on_data, data);
    }
}

unsafe extern "C" fn fader_on_volume(data: *mut c_void, calldata: *mut c_void) {
    // SAFETY: `data` is the host pointer passed to `signal_handler_connect`.
    let host = unsafe { &*data.cast::<FaderHost>() };
    // SAFETY: `calldata` is the signal argument.
    let mul = unsafe { calldata_volume(calldata) };
    host.fader.on_volume(mul);
}

unsafe extern "C" fn fader_on_destroy(data: *mut c_void, _calldata: *mut c_void) {
    // SAFETY: `data` is the host pointer passed to `signal_handler_connect`.
    let host = unsafe { &*data.cast::<FaderHost>() };
    detach_fader(host);
}

unsafe extern "C" fn meter_on_volume(data: *mut c_void, calldata: *mut c_void) {
    // SAFETY: `data` is the host pointer passed to `signal_handler_connect`.
    let host = unsafe { &*data.cast::<MeterHost>() };
    // SAFETY: `calldata` is the signal argument.
    let mul = unsafe { calldata_volume(calldata) };
    host.meter.on_volume(mul);
}

unsafe extern "C" fn meter_on_destroy(data: *mut c_void, _calldata: *mut c_void) {
    // SAFETY: `data` is the host pointer passed to `signal_handler_connect`.
    let host = unsafe { &*data.cast::<MeterHost>() };
    detach_meter(host);
}

unsafe extern "C" fn meter_on_data(
    data: *mut c_void,
    source: *mut c_void,
    audio: *const audio_data,
    muted: bool,
) {
    // SAFETY: `data` is the host pointer registered with the capture callback.
    let host = unsafe { &*data.cast::<MeterHost>() };
    // SAFETY: `audio` is the capture packet. Planes are readable for `frames`.
    let audio = unsafe { &*audio };
    let frames = audio.frames as usize;
    let mut planes = [None; MAX_AUDIO_CHANNELS];
    let mut aligned = [true; MAX_AUDIO_CHANNELS];
    for (plane_nr, ptr) in audio.data.iter().copied().enumerate() {
        if plane_nr >= MAX_AUDIO_CHANNELS || ptr.is_null() {
            continue;
        }
        let samples = ptr.cast::<f32>();
        if (samples as usize & 0xf) > 0 {
            aligned[plane_nr] = false;
            // SAFETY: the format string is a literal. `samples` is the plane.
            unsafe {
                printf(
                    c"Audio plane %i is not aligned %p skipping peak volume measurement.\n"
                        .as_ptr(),
                    plane_nr as c_int,
                    samples.cast::<c_void>(),
                );
            }
        }
        // SAFETY: the caller, like C, guarantees `frames` samples at `samples`.
        planes[plane_nr] = Some(unsafe { std::slice::from_raw_parts(samples, frames) });
    }
    let (speakers, user_muted) = if source.is_null() {
        (0, false)
    } else {
        // SAFETY: `source` is the source obs passed into the capture callback.
        unsafe {
            (
                obs_source_get_speaker_layout(source),
                obs_source_muted(source),
            )
        }
    };
    host.meter.on_data(&AudioPacket {
        planes,
        frames,
        speakers,
        aligned,
        capture_muted: muted,
        user_muted,
    });
}

/// # Safety
/// Same contract as C `obs_fader_create`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_create(ty: c_int) -> *mut c_void {
    // SAFETY: one pointer. A failed type frees it, so the net count is 0.
    let mem = unsafe { bzalloc(size_of::<*mut FaderHost>()) };
    if mem.is_null() {
        return ptr::null_mut();
    }
    let Some(fader) = Fader::create(ty) else {
        // SAFETY: `mem` is the block just allocated.
        unsafe { bfree(mem) };
        return ptr::null_mut();
    };
    let host = Box::into_raw(Box::new(FaderHost {
        fader,
        source: Mutex::new(0),
        slots: Mutex::new(Vec::new()),
    }));
    // SAFETY: `mem` has room for one pointer.
    unsafe { mem.cast::<*mut FaderHost>().write(host) };
    mem
}

/// # Safety
/// Same contract as C `obs_fader_destroy`. Null returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_destroy(fader: *mut c_void) {
    if fader.is_null() {
        return;
    }
    // SAFETY: `fader` is a live handle from `obs_fader_create`.
    let host = unsafe { fader.cast::<*mut FaderHost>().read() };
    detach_fader(unsafe { &*host });
    drop(unsafe { Box::from_raw(host) });
    // SAFETY: `fader` is the block from create.
    unsafe { bfree(fader) };
}

/// # Safety
/// Same contract as C `obs_fader_set_db`. Null returns false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_set_db(fader: *mut c_void, db: f32) -> bool {
    if fader.is_null() {
        return false;
    }
    // SAFETY: `fader` is a live handle.
    let host = unsafe { fader_host(fader) };
    let stored = host.fader.set_db(db);
    push_volume(host);
    stored
}

/// # Safety
/// Same contract as C `obs_fader_get_db`. Null returns 0.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_get_db(fader: *mut c_void) -> f32 {
    if fader.is_null() {
        return 0.0;
    }
    // SAFETY: `fader` is a live handle.
    unsafe { fader_host(fader) }.fader.db()
}

/// # Safety
/// Same contract as C `obs_fader_set_deflection`. Null returns false.
/// One `obs_source_set_volume`, because C calls `obs_fader_set_db`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_set_deflection(fader: *mut c_void, def: f32) -> bool {
    if fader.is_null() {
        return false;
    }
    // SAFETY: `fader` is a live handle.
    let host = unsafe { fader_host(fader) };
    let stored = host.fader.set_deflection(def);
    push_volume(host);
    stored
}

/// # Safety
/// Same contract as C `obs_fader_get_deflection`. Null returns 0.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_get_deflection(fader: *mut c_void) -> f32 {
    if fader.is_null() {
        return 0.0;
    }
    // SAFETY: `fader` is a live handle.
    unsafe { fader_host(fader) }.fader.deflection()
}

/// # Safety
/// Same contract as C `obs_fader_set_mul`. Null returns false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_set_mul(fader: *mut c_void, mul: f32) -> bool {
    if fader.is_null() {
        return false;
    }
    // SAFETY: `fader` is a live handle.
    let host = unsafe { fader_host(fader) };
    let stored = host.fader.set_mul(mul);
    push_volume(host);
    stored
}

/// # Safety
/// Same contract as C `obs_fader_get_mul`. Null returns 0.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_get_mul(fader: *mut c_void) -> f32 {
    if fader.is_null() {
        return 0.0;
    }
    // SAFETY: `fader` is a live handle.
    unsafe { fader_host(fader) }.fader.mul()
}

/// # Safety
/// Same contract as C `obs_fader_attach_source`. A null fader or source
/// returns false. Does not take a source reference.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_attach_source(fader: *mut c_void, source: *mut c_void) -> bool {
    if fader.is_null() || source.is_null() {
        return false;
    }
    // SAFETY: `fader` is a live handle. `source` is a live source.
    let host = unsafe { fader_host(fader) };
    detach_fader(host);
    let data = host_data(host);
    // SAFETY: connect runs without the source mutex held. The data pointer
    // is the host `Box`, stable until destroy.
    let volume = unsafe {
        let handler = obs_source_get_signal_handler(source);
        signal_handler_connect(handler, c"volume".as_ptr(), fader_on_volume, data);
        signal_handler_connect(handler, c"destroy".as_ptr(), fader_on_destroy, data);
        obs_source_get_volume(source)
    };
    *host.source.lock().expect("fader source") = source as usize;
    host.fader.attach_volume(volume);
    true
}

/// # Safety
/// Same contract as C `obs_fader_detach_source`. Null returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_detach_source(fader: *mut c_void) {
    if fader.is_null() {
        return;
    }
    // SAFETY: `fader` is a live handle.
    detach_fader(unsafe { fader_host(fader) });
}

/// # Safety
/// Same contract as C `obs_fader_add_callback`. A null fader logs and returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_add_callback(
    fader: *mut c_void,
    callback: FaderFn,
    param: *mut c_void,
) {
    // SAFETY: null logs through `blog`. A live fader is a handle from create.
    if !unsafe { ptr_valid(fader, c"obs_fader_add_callback", c"fader") } {
        return;
    }
    let host = unsafe { fader_host(fader) };
    let slot = Box::new(FaderSlot {
        callback,
        param: param as usize,
    });
    let addr = ptr::from_ref(slot.as_ref()) as usize;
    host.slots.lock().expect("fader slots").push(slot);
    host.fader.add_callback(fader_trampoline as FaderCb, addr);
}

/// # Safety
/// Same contract as C `obs_fader_remove_callback`. One matching pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_remove_callback(
    fader: *mut c_void,
    callback: FaderFn,
    param: *mut c_void,
) {
    // SAFETY: null logs through `blog`. A live fader is a handle from create.
    if !unsafe { ptr_valid(fader, c"obs_fader_remove_callback", c"fader") } {
        return;
    }
    let host = unsafe { fader_host(fader) };
    let param = param as usize;
    let mut slots = host.slots.lock().expect("fader slots");
    let Some(index) = slots
        .iter()
        .position(|slot| same_fader_fn(slot.callback, callback) && slot.param == param)
    else {
        return;
    };
    let addr = ptr::from_ref(slots[index].as_ref()) as usize;
    host.fader
        .remove_callback(fader_trampoline as FaderCb, addr);
    slots.remove(index);
}

/// # Safety
/// Same contract as C `obs_volmeter_create`. Any type is kept.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_create(ty: c_int) -> *mut c_void {
    // SAFETY: one pointer. The meter is kept for every type, including 99.
    let mem = unsafe { bzalloc(size_of::<*mut MeterHost>()) };
    if mem.is_null() {
        return ptr::null_mut();
    }
    let host = Box::into_raw(Box::new(MeterHost {
        meter: Volmeter::create(ty),
        source: Mutex::new(0),
        slots: Mutex::new(Vec::new()),
    }));
    // SAFETY: `mem` has room for one pointer.
    unsafe { mem.cast::<*mut MeterHost>().write(host) };
    mem
}

/// # Safety
/// Same contract as C `obs_volmeter_destroy`. Null returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_destroy(volmeter: *mut c_void) {
    if volmeter.is_null() {
        return;
    }
    // SAFETY: `volmeter` is a live handle from `obs_volmeter_create`.
    let host = unsafe { volmeter.cast::<*mut MeterHost>().read() };
    detach_meter(unsafe { &*host });
    drop(unsafe { Box::from_raw(host) });
    // SAFETY: `volmeter` is the block from create.
    unsafe { bfree(volmeter) };
}

/// # Safety
/// Same contract as C `obs_volmeter_attach_source`. A null meter or source
/// returns false.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_attach_source(
    volmeter: *mut c_void,
    source: *mut c_void,
) -> bool {
    if volmeter.is_null() || source.is_null() {
        return false;
    }
    // SAFETY: `volmeter` is a live handle. `source` is a live source.
    let host = unsafe { meter_host(volmeter) };
    detach_meter(host);
    let data = host_data(host);
    // SAFETY: connect and the capture registration run without the source
    // mutex held. The data pointer is the host `Box`.
    let volume = unsafe {
        let handler = obs_source_get_signal_handler(source);
        signal_handler_connect(handler, c"volume".as_ptr(), meter_on_volume, data);
        signal_handler_connect(handler, c"destroy".as_ptr(), meter_on_destroy, data);
        obs_source_add_audio_capture_callback(source, meter_on_data, data);
        obs_source_get_volume(source)
    };
    *host.source.lock().expect("meter source") = source as usize;
    host.meter.attach_volume(volume);
    true
}

/// # Safety
/// Same contract as C `obs_volmeter_detach_source`. Null returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_detach_source(volmeter: *mut c_void) {
    if volmeter.is_null() {
        return;
    }
    // SAFETY: `volmeter` is a live handle.
    detach_meter(unsafe { meter_host(volmeter) });
}

/// # Safety
/// Same contract as C `obs_volmeter_set_peak_meter_type`. Null is not checked.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_set_peak_meter_type(
    volmeter: *mut c_void,
    peak_meter_type: c_int,
) {
    // SAFETY: `volmeter` is a live handle. C crashes on null.
    unsafe { meter_host(volmeter) }
        .meter
        .set_peak_meter_type(peak_meter_type);
}

/// # Safety
/// Same contract as C `obs_volmeter_get_nr_channels`. Null is not checked.
/// The speaker layout is `obs_source_get_speaker_layout`, which reads
/// `sample_info.speakers`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_get_nr_channels(volmeter: *mut c_void) -> c_int {
    // SAFETY: `volmeter` is a live handle. C crashes on null.
    let host = unsafe { meter_host(volmeter) };
    let source = *host.source.lock().expect("meter source");
    if source != 0 {
        // SAFETY: `source` is the pointer `attach` stored.
        let speakers = unsafe { obs_source_get_speaker_layout(source as *mut c_void) };
        host.meter.note_speakers(speakers);
    }
    let mut info = ObsAudioInfo {
        samples_per_sec: 0,
        speakers: 0,
    };
    // SAFETY: `info` is a live `obs_audio_info`.
    let mix = if unsafe { obs_get_audio_info(ptr::from_mut(&mut info)) } {
        Some(info.speakers)
    } else {
        None
    };
    host.meter.nr_channels(mix)
}

/// # Safety
/// Same contract as C `obs_volmeter_add_callback`. A null meter logs and returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_add_callback(
    volmeter: *mut c_void,
    callback: MeterFn,
    param: *mut c_void,
) {
    // SAFETY: null logs through `blog`. A live meter is a handle from create.
    if !unsafe { ptr_valid(volmeter, c"obs_volmeter_add_callback", c"volmeter") } {
        return;
    }
    let host = unsafe { meter_host(volmeter) };
    let slot = Box::new(MeterSlot {
        callback,
        param: param as usize,
    });
    let addr = ptr::from_ref(slot.as_ref()) as usize;
    host.slots.lock().expect("meter slots").push(slot);
    host.meter.add_callback(meter_trampoline as MeterCb, addr);
}

/// # Safety
/// Same contract as C `obs_volmeter_remove_callback`. One matching pair.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_volmeter_remove_callback(
    volmeter: *mut c_void,
    callback: MeterFn,
    param: *mut c_void,
) {
    // SAFETY: null logs through `blog`. A live meter is a handle from create.
    if !unsafe { ptr_valid(volmeter, c"obs_volmeter_remove_callback", c"volmeter") } {
        return;
    }
    let host = unsafe { meter_host(volmeter) };
    let param = param as usize;
    let mut slots = host.slots.lock().expect("meter slots");
    let Some(index) = slots
        .iter()
        .position(|slot| same_meter_fn(slot.callback, callback) && slot.param == param)
    else {
        return;
    };
    let addr = ptr::from_ref(slots[index].as_ref()) as usize;
    host.meter
        .remove_callback(meter_trampoline as MeterCb, addr);
    slots.remove(index);
}

/// # Safety
/// Same contract as C `obs_mul_to_db`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_mul_to_db(mul: f32) -> f32 {
    mul_to_db(mul)
}

/// # Safety
/// Same contract as C `obs_db_to_mul`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_db_to_mul(db: f32) -> f32 {
    db_to_mul(db)
}

/// # Safety
/// Same contract as C `obs_fader_db_to_def`. Null is not checked. The
/// returned function is not exported.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn obs_fader_db_to_def(fader: *mut c_void) -> extern "C" fn(f32) -> f32 {
    // SAFETY: `fader` is a live handle. C crashes on null.
    match unsafe { fader_host(fader) }.fader.kind() {
        FADER_IEC => iec_db_to_def,
        FADER_LOG => log_db_to_def,
        _ => cubic_db_to_def,
    }
}
