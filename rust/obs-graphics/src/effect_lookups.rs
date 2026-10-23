//! Safe core for `libobs/graphics/effect-lookups.c.h`.
//!
//! Name walks are a first-match `strcmp` on the bytes before the NUL.
//! Param indexes stop at the count (`>=` is a miss). Annotation indexes
//! and pass indexes use `>`, so an index equal to the count is one past
//! the array. That return is characterized, not endorsed.

/// Where an annotation or pass index lands.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LooseIndex {
    /// `index < len`.
    At(usize),
    /// `index == len`. C returns `array + len` and does not dereference it.
    OnePast,
    /// Null owner, or `index > len`.
    Miss,
}

/// `gs_effect_get_num_params` / `gs_param_get_num_annotations`.
pub fn count(present: bool, len: usize) -> usize {
    if present { len } else { 0 }
}

/// `gs_effect_get_param_by_idx`. `index >= len` is a miss.
pub fn param_index(present: bool, len: usize, index: usize) -> Option<usize> {
    if present && index < len {
        Some(index)
    } else {
        None
    }
}

/// Pointer `gs_effect_get_param_by_idx` returns. A miss is null.
pub(crate) fn param_at<T>(present: bool, ptr: *const T, len: usize, index: usize) -> *const T {
    match param_index(present, len, index) {
        Some(i) => ptr.wrapping_add(i),
        None => core::ptr::null(),
    }
}

/// `gs_param_get_annotation_by_idx` / `gs_technique_get_pass_by_idx`.
pub fn loose_index(present: bool, len: usize, index: usize) -> LooseIndex {
    if !present || index > len {
        LooseIndex::Miss
    } else if index == len {
        LooseIndex::OnePast
    } else {
        LooseIndex::At(index)
    }
}

/// Pointer the annotation and pass index lookups return.
///
/// `OnePast` is `ptr + len`, including when `ptr` is null and `len` is 0,
/// which is a null pointer. C does the same add and does not dereference it.
pub(crate) fn loose_at<T>(present: bool, ptr: *const T, len: usize, index: usize) -> *const T {
    match loose_index(present, len, index) {
        LooseIndex::At(i) => ptr.wrapping_add(i),
        LooseIndex::OnePast => ptr.wrapping_add(len),
        LooseIndex::Miss => core::ptr::null(),
    }
}

/// First exact byte match. The owner being absent does not read `name_at`.
pub(crate) fn find_at<'a>(
    present: bool,
    len: usize,
    name: &[u8],
    mut name_at: impl FnMut(usize) -> &'a [u8],
) -> Option<usize> {
    if !present {
        return None;
    }
    (0..len).find(|&i| name_at(i) == name)
}

/// First exact byte match, or `None` when the owner is null or the name
/// is missing. `gs_effect_get_param_by_name`, `gs_param_get_annotation_by_name`,
/// `gs_effect_get_technique`, `gs_technique_get_pass_by_name`.
pub fn find_name(present: bool, names: &[&[u8]], name: &[u8]) -> Option<usize> {
    find_at(present, names.len(), name, |i| names[i])
}

/// `gs_effect_get_current_technique`, `gs_effect_get_viewproj_matrix`,
/// `gs_effect_get_world_matrix`. A null owner returns null. Otherwise the
/// stored pointer is returned as it is, including a pointer that is not
/// in the owner's array.
pub fn stored<T>(present: bool, value: *const T) -> *const T {
    if present { value } else { core::ptr::null() }
}

/// Name pointer and type copied by `gs_effect_get_param_info`.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct ParamInfo<'a> {
    pub name: &'a [u8],
    pub type_: i32,
}

/// `gs_effect_get_param_info`. `None` means the param was null and the
/// caller must leave `info` untouched.
pub fn param_info(param: Option<(&[u8], i32)>) -> Option<ParamInfo<'_>> {
    param.map(|(name, type_)| ParamInfo { name, type_ })
}
