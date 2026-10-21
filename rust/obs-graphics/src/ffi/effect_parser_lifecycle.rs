//! Hidden ep_free ABI. Only the parent layout crosses this adapter; element
//! layouts and their existing header-inline destructors stay in C.

use crate::effect_parser_lifecycle::{self as safe, Collection, Parser};
use core::ffi::c_void;
use core::ptr;
use obs_util::ffi::cf_parser::cf_parser;
use obs_util::ffi::darray::darray;

/// Exact layout of graphics/effect-parser.h; no fields are added or removed.
#[repr(C)]
#[allow(non_camel_case_types)]
pub struct effect_parser {
    pub effect: *mut c_void,
    pub params: darray,
    pub structs: darray,
    pub funcs: darray,
    pub samplers: darray,
    pub techniques: darray,
    pub files: darray,
    pub tokens: darray,
    pub cur_pass: *mut c_void,
    pub cfp: cf_parser,
}

unsafe extern "C" {
    fn obs_effect_parser_destroy_element(ep: *mut effect_parser, collection: usize, index: usize);
    fn obs_effect_parser_destroy_state(ep: *mut effect_parser);
    fn obs_effect_parser_release_storage(ep: *mut effect_parser, collection: usize);
}

// This wrapper is constructed only under ep_free's exclusive ownership contract.
struct RawParser<'a>(&'a mut effect_parser);

impl RawParser<'_> {
    fn array(&self, collection: Collection) -> &darray {
        match collection {
            Collection::Parameters => &self.0.params,
            Collection::Structures => &self.0.structs,
            Collection::Functions => &self.0.funcs,
            Collection::Samplers => &self.0.samplers,
            Collection::Techniques => &self.0.techniques,
        }
    }
}

impl Parser for RawParser<'_> {
    fn len(&self, collection: Collection) -> usize {
        self.array(collection).num
    }
    fn destroy_element(&mut self, collection: Collection, index: usize) {
        // SAFETY: the core visits each initialized element once, before freeing
        // any collection storage. The C adapter uses the unchanged inline destructor.
        unsafe { obs_effect_parser_destroy_element(self.0, collection as usize, index) };
    }
    fn clear_current_pass(&mut self) {
        self.0.cur_pass = ptr::null_mut();
    }
    fn destroy_parser_state(&mut self) {
        // SAFETY: cfp is initialized and exclusively owned; cf_parser_free
        // releases its lexer/preprocessor/errors and clears its cursor.
        unsafe { obs_effect_parser_destroy_state(self.0) };
    }
    fn release_storage(&mut self, collection: Collection) {
        // SAFETY: elements have been destroyed, and the buffer is owned bmem
        // storage. The C da_free adapter uses the same linked allocator as
        // the retained inline destructors, clearing pointer/length/capacity.
        unsafe { obs_effect_parser_release_storage(self.0, collection as usize) };
    }
}

/// # Safety
/// `ep` must be non-null, initialized and exclusively borrowed; its owned
/// allocations must be valid, distinct bmem allocations. Borrowed effect,
/// property and dependency pointers must remain valid until cleanup returns.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn ep_free(ep: *mut effect_parser) {
    // SAFETY: the caller supplies the same live parser required by C ep_free.
    safe::free(&mut RawParser(unsafe { &mut *ep }));
}
