//! ABI adapter for graphics/shader-parser.h. Native strtol remains the numeric
//! conversion boundary, preserving long width, locale, overflow and errno.
use crate::shader_sampler_convert::{self as safe, Sampler};
use core::ffi::{CStr, c_char};
use obs_util::ffi::darray::darray;

#[repr(C)]
#[allow(non_camel_case_types)]
pub struct shader_sampler {
    pub name: *mut c_char,
    pub states: darray,
    pub values: darray,
}

#[repr(C)]
#[allow(non_camel_case_types)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct gs_sampler_info {
    pub filter: i32,
    pub address_u: i32,
    pub address_v: i32,
    pub address_w: i32,
    pub max_anisotropy: i32,
    pub border_color: u32,
}

struct BorrowedSampler<'a>(&'a shader_sampler);
impl BorrowedSampler<'_> {
    fn text(&self, values: &darray, index: usize) -> &CStr {
        // SAFETY: the exported function's caller supplies initialized pointer
        // arrays and NUL-terminated strings at each visited index.
        let text = unsafe { *values.array.cast::<*const c_char>().add(index) };
        if text.is_null() {
            c""
        } else {
            // SAFETY: every non-null element is a valid C string.
            unsafe { CStr::from_ptr(text) }
        }
    }
}
impl Sampler for BorrowedSampler<'_> {
    fn state_count(&self) -> usize {
        self.0.states.num
    }
    fn state(&self, index: usize) -> &[u8] {
        self.text(&self.0.states, index).to_bytes()
    }
    fn value(&self, index: usize) -> &[u8] {
        self.text(&self.0.values, index).to_bytes()
    }
    fn number(&self, index: usize, offset: usize, radix: u32) -> i128 {
        let value = self.text(&self.0.values, index);
        // SAFETY: offset is 0 for decimal or 1 for a nonempty border string;
        // both retain the terminating NUL. This is the original C strtol call.
        i128::from(unsafe {
            libc::strtol(
                value.as_ptr().add(offset),
                core::ptr::null_mut(),
                radix as i32,
            )
        })
    }
}

/// # Safety
/// `sampler` is initialized, with equally sized arrays of valid C strings; null states are ignored and
/// null address values map to CLAMP, as with C astrcmpi. Numeric and Filter
/// values must be non-null;
/// BorderColor values contain at least one byte before NUL. `info` is writable
/// and does not overlap the sampler or its strings/arrays, as in the C API.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn shader_sampler_convert(
    sampler: *mut shader_sampler,
    info: *mut gs_sampler_info,
) {
    // SAFETY: sampler is initialized and borrowed only for this call.
    let result = safe::convert(&BorrowedSampler(unsafe { &*sampler }));
    // SAFETY: info is a valid exclusive output pointer. All six C fields are written.
    unsafe {
        info.write(gs_sampler_info {
            filter: result.filter,
            address_u: result.address_u,
            address_v: result.address_v,
            address_w: result.address_w,
            max_anisotropy: result.max_anisotropy,
            border_color: result.border_color,
        })
    };
}
