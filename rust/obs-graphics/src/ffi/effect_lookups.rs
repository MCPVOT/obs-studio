//! C ABI for the lookups in `libobs/graphics/effect-lookups.c.h`.
//!
//! The structs mirror `graphics/effect.h`. The shim reads fields and
//! calls the safe core; the index rules, including the one-past annotation
//! and pass return, live in the core.

use crate::effect_lookups::{self as safe};
use core::ffi::{CStr, c_char, c_void};
use obs_util::ffi::darray::darray;

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct gs_effect_param {
    pub name: *mut c_char,
    pub section: i32,
    pub type_: i32,
    pub changed: bool,
    pub cur_val: darray,
    pub default_val: darray,
    pub effect: *mut gs_effect,
    pub next_sampler: *mut c_void,
    pub annotations: darray,
}

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct gs_effect_pass {
    pub name: *mut c_char,
    pub section: i32,
    pub vertshader: *mut c_void,
    pub pixelshader: *mut c_void,
    pub vertshader_params: darray,
    pub pixelshader_params: darray,
}

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct gs_effect_technique {
    pub name: *mut c_char,
    pub section: i32,
    pub effect: *mut gs_effect,
    pub passes: darray,
}

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct gs_effect {
    pub processing: bool,
    pub cached: bool,
    pub effect_path: *mut c_char,
    pub effect_dir: *mut c_char,
    pub params: darray,
    pub techniques: darray,
    pub cur_technique: *mut gs_effect_technique,
    pub cur_pass: *mut gs_effect_pass,
    pub view_proj: *mut gs_effect_param,
    pub world: *mut gs_effect_param,
    pub scale: *mut gs_effect_param,
    pub graphics: *mut c_void,
    pub next: *mut gs_effect,
    pub loop_pass: usize,
    pub looping: bool,
}

#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy)]
pub struct gs_effect_param_info {
    pub name: *const c_char,
    pub type_: i32,
}

/// # Safety
/// `name` must be a valid NUL-terminated string.
unsafe fn bytes<'a>(name: *const c_char) -> &'a [u8] {
    // SAFETY: the caller passes a NUL-terminated string.
    unsafe { CStr::from_ptr(name) }.to_bytes()
}

/// # Safety
/// When `present` is true, `base` points at `len` initialized elements and
/// `name` is a valid NUL-terminated string. Each name `field` returns is
/// also a valid NUL-terminated string.
unsafe fn lookup<T>(
    present: bool,
    base: *const T,
    len: usize,
    name: *const c_char,
    field: impl Fn(*const T) -> *const c_char,
) -> *mut T {
    if !present {
        return core::ptr::null_mut();
    }
    // SAFETY: `name` is a C string when the owner is present. C calls
    // strcmp on it in that same case.
    let wanted = unsafe { bytes(name) };
    match safe::find_at(true, len, wanted, |i| {
        let item = base.wrapping_add(i);
        // SAFETY: `i < len`, so `item` is one of the initialized elements,
        // and its name is a C string.
        let found = field(item);
        unsafe { bytes(found) }
    }) {
        Some(i) => base.wrapping_add(i) as *mut T,
        None => core::ptr::null_mut(),
    }
}

/// # Safety
/// `effect` is null or a live `gs_effect`. `name` is a C string when
/// `effect` is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_technique(
    effect: *const gs_effect,
    name: *const c_char,
) -> *mut gs_effect_technique {
    if effect.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: `effect` is live.
    let effect = unsafe { &*effect };
    let base = effect.techniques.array as *const gs_effect_technique;
    // SAFETY: the technique array has `num` elements, each with a C string name.
    unsafe { lookup(true, base, effect.techniques.num, name, |item| (*item).name) }
}

/// # Safety
/// `effect` is null or a live `gs_effect`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_current_technique(
    effect: *const gs_effect,
) -> *mut gs_effect_technique {
    if effect.is_null() {
        return safe::stored(false, core::ptr::null::<gs_effect_technique>()) as *mut _;
    }
    // SAFETY: `effect` is live. The stored technique may be outside the array.
    let cur = unsafe { (*effect).cur_technique };
    safe::stored(true, cur) as *mut _
}

/// # Safety
/// `effect` is null or a live `gs_effect`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_num_params(effect: *const gs_effect) -> usize {
    if effect.is_null() {
        safe::count(false, 0)
    } else {
        // SAFETY: `effect` is live.
        safe::count(true, unsafe { (*effect).params.num })
    }
}

/// # Safety
/// `effect` is null or a live `gs_effect`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_param_by_idx(
    effect: *const gs_effect,
    param: usize,
) -> *mut gs_effect_param {
    if effect.is_null() {
        return safe::param_at(false, core::ptr::null::<gs_effect_param>(), 0, param) as *mut _;
    }
    // SAFETY: `effect` is live.
    let effect = unsafe { &*effect };
    let base = effect.params.array as *const gs_effect_param;
    safe::param_at(true, base, effect.params.num, param) as *mut _
}

/// # Safety
/// `effect` is null or a live `gs_effect`. `name` is a C string when
/// `effect` is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_param_by_name(
    effect: *const gs_effect,
    name: *const c_char,
) -> *mut gs_effect_param {
    if effect.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: `effect` is live.
    let effect = unsafe { &*effect };
    let base = effect.params.array as *const gs_effect_param;
    // SAFETY: the param array has `num` elements, each with a C string name.
    unsafe { lookup(true, base, effect.params.num, name, |item| (*item).name) }
}

/// # Safety
/// `param` is null or a live `gs_effect_param`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_param_get_num_annotations(param: *const gs_effect_param) -> usize {
    if param.is_null() {
        safe::count(false, 0)
    } else {
        // SAFETY: `param` is live.
        safe::count(true, unsafe { (*param).annotations.num })
    }
}

/// # Safety
/// `param` is null or a live `gs_effect_param`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_param_get_annotation_by_idx(
    param: *const gs_effect_param,
    annotation: usize,
) -> *mut gs_effect_param {
    if param.is_null() {
        return safe::loose_at(false, core::ptr::null::<gs_effect_param>(), 0, annotation)
            as *mut _;
    }
    // SAFETY: `param` is live.
    let param = unsafe { &*param };
    let base = param.annotations.array as *const gs_effect_param;
    safe::loose_at(true, base, param.annotations.num, annotation) as *mut _
}

/// # Safety
/// `param` is null or a live `gs_effect_param`. `name` is a C string when
/// `param` is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_param_get_annotation_by_name(
    param: *const gs_effect_param,
    name: *const c_char,
) -> *mut gs_effect_param {
    if param.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: `param` is live.
    let param = unsafe { &*param };
    let base = param.annotations.array as *const gs_effect_param;
    // SAFETY: the annotation array has `num` elements, each with a C string name.
    unsafe { lookup(true, base, param.annotations.num, name, |item| (*item).name) }
}

/// # Safety
/// `technique` is null or a live `gs_effect_technique`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_technique_get_pass_by_idx(
    technique: *const gs_effect_technique,
    pass: usize,
) -> *mut gs_effect_pass {
    if technique.is_null() {
        return safe::loose_at(false, core::ptr::null::<gs_effect_pass>(), 0, pass) as *mut _;
    }
    // SAFETY: `technique` is live.
    let technique = unsafe { &*technique };
    let base = technique.passes.array as *const gs_effect_pass;
    safe::loose_at(true, base, technique.passes.num, pass) as *mut _
}

/// # Safety
/// `technique` is null or a live `gs_effect_technique`. `name` is a C string
/// when `technique` is non-null.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_technique_get_pass_by_name(
    technique: *const gs_effect_technique,
    name: *const c_char,
) -> *mut gs_effect_pass {
    if technique.is_null() {
        return core::ptr::null_mut();
    }
    // SAFETY: `technique` is live.
    let technique = unsafe { &*technique };
    let base = technique.passes.array as *const gs_effect_pass;
    // SAFETY: the pass array has `num` elements, each with a C string name.
    unsafe { lookup(true, base, technique.passes.num, name, |item| (*item).name) }
}

/// # Safety
/// `effect` is null or a live `gs_effect`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_viewproj_matrix(
    effect: *const gs_effect,
) -> *mut gs_effect_param {
    if effect.is_null() {
        return safe::stored(false, core::ptr::null::<gs_effect_param>()) as *mut _;
    }
    // SAFETY: `effect` is live.
    let view = unsafe { (*effect).view_proj };
    safe::stored(true, view) as *mut _
}

/// # Safety
/// `effect` is null or a live `gs_effect`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_world_matrix(
    effect: *const gs_effect,
) -> *mut gs_effect_param {
    if effect.is_null() {
        return safe::stored(false, core::ptr::null::<gs_effect_param>()) as *mut _;
    }
    // SAFETY: `effect` is live.
    let world = unsafe { (*effect).world };
    safe::stored(true, world) as *mut _
}

/// # Safety
/// `param` is null or a live `gs_effect_param`. When `param` is non-null,
/// `info` is a live `gs_effect_param_info`. A null `param` does not read
/// `info`.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_effect_get_param_info(
    param: *const gs_effect_param,
    info: *mut gs_effect_param_info,
) {
    let fields = if param.is_null() {
        None
    } else {
        // SAFETY: `param` is live. A null name is copied as a pointer and
        // is not read; C does the same.
        let param = unsafe { &*param };
        let name = if param.name.is_null() {
            &[][..]
        } else {
            unsafe { bytes(param.name) }
        };
        Some((name, param.type_))
    };
    if let Some(got) = safe::param_info(fields) {
        // SAFETY: `param` is non-null when `fields` is Some, and `info` is live.
        let param = unsafe { &*param };
        let info = unsafe { &mut *info };
        info.name = param.name;
        info.type_ = got.type_;
    }
}
