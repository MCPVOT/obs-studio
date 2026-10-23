//! C ABI for `libobs/graphics/image-file.c.h`.
//!
//! `struct gs_image_file_ex` (public, `graphics/image-file.h`) is
//! [`gs_image_file_ex_t`], the safe core's [`ImageFileEx`] over the real
//! pointers. `struct gs_image_file_internal` stays private: the Rust block
//! is a `bzalloc` allocation as in the C, so `bnum_allocs` counts the same
//! objects. libnsgif, `gs_create_texture_file_data3` and the `gs_texture_*`
//! calls stay C: libobs links them, and cargo tests get libnsgif and a
//! recording `gs_*` stand-in from the oracle crate.

use core::ffi::{CStr, c_char, c_int, c_void};
use core::ptr::{self, NonNull};

// bmalloc and bzalloc abort instead of returning null, so the dangling
// fallbacks below are never taken; they keep this file free of panics.

use crate::image_file::{FileRead, GifInfo, Host, ImageAlphaMode, ImageFileEx, Internal};
use crate::texture_render::{ColorFormat, ColorSpace};

/// `nsgif_bitmap_cb_vt`.
#[repr(C)]
struct NsgifBitmapCbVt {
    create: Option<unsafe extern "C" fn(c_int, c_int) -> *mut c_void>,
    destroy: Option<unsafe extern "C" fn(*mut c_void)>,
    get_buffer: Option<unsafe extern "C" fn(*mut c_void) -> *mut u8>,
    set_opaque: Option<unsafe extern "C" fn(*mut c_void, bool)>,
    test_opaque: Option<unsafe extern "C" fn(*mut c_void) -> bool>,
    modified: Option<unsafe extern "C" fn(*mut c_void)>,
    get_rowspan: Option<unsafe extern "C" fn(*mut c_void) -> u32>,
}

/// The `nsgif_info_t` fields up to the ones the file reads.
#[repr(C)]
struct NsgifInfo {
    width: u32,
    height: u32,
    frame_count: u32,
    loop_max: c_int,
}

/// `nsgif_frame_info_t` up to `delay`.
#[repr(C)]
struct NsgifFrameInfo {
    display: bool,
    transparency: bool,
    local_palette: bool,
    interlaced: bool,
    disposal: u8,
    delay: u32,
}

/// `NSGIF_BITMAP_FMT_R8G8B8A8`.
const NSGIF_BITMAP_FMT_R8G8B8A8: c_int = 0;

unsafe extern "C" {
    fn nsgif_create(
        bitmap_vt: *const NsgifBitmapCbVt,
        bitmap_fmt: c_int,
        gif_out: *mut *mut c_void,
    ) -> u32;
    fn nsgif_destroy(gif: *mut c_void);
    fn nsgif_data_scan(gif: *mut c_void, size: usize, data: *const u8) -> u32;
    fn nsgif_data_complete(gif: *mut c_void);
    fn nsgif_get_info(gif: *const c_void) -> *const NsgifInfo;
    fn nsgif_get_frame_info(gif: *const c_void, frame: u32) -> *const NsgifFrameInfo;
    fn nsgif_frame_decode(gif: *mut c_void, frame: u32, bitmap: *mut *mut c_void) -> u32;

    fn gs_create_texture_file_data3(
        file: *const c_char,
        alpha_mode: ImageAlphaMode,
        format: *mut ColorFormat,
        cx_out: *mut u32,
        cy_out: *mut u32,
        space: *mut ColorSpace,
    ) -> *mut u8;
    fn gs_texture_create(
        width: u32,
        height: u32,
        color_format: ColorFormat,
        levels: u32,
        data: *const *const u8,
        flags: u32,
    ) -> *mut c_void;
    fn gs_texture_destroy(tex: *mut c_void);
    fn gs_texture_set_image(tex: *mut c_void, data: *const u8, linesize: u32, flip: bool);

    // libobs's allocator, log and file helpers, through the C adapters in
    // libobs/graphics/obs-graphics-adapter.c.h. Nothing here may name a symbol
    // obs-util also defines: an executable that links the staticlib next to
    // libobs (test_effect_parser_lifecycle_rust) would then pull obs-util
    // objects from the staticlib, which clash with libobs's exports on
    // Windows.
    #[link_name = "obs_graphics_log"]
    fn log_line(level: c_int, line: *const c_char);
    #[link_name = "obs_graphics_bmalloc"]
    fn bmalloc(size: usize) -> *mut c_void;
    #[link_name = "obs_graphics_bfree"]
    fn bfree(ptr: *mut c_void);
    #[link_name = "obs_graphics_fopen_rb"]
    fn fopen_rb(path: *const c_char) -> *mut c_void;
    #[link_name = "obs_graphics_ftelli64"]
    fn os_ftelli64(file: *mut c_void) -> i64;
}

unsafe extern "C" fn bi_def_bitmap_create(width: c_int, height: c_int) -> *mut c_void {
    // SAFETY: a plain allocation, sized as the C sizes it.
    unsafe {
        bmalloc(
            4usize
                .wrapping_mul(width as usize)
                .wrapping_mul(height as usize),
        )
    }
}

unsafe extern "C" fn bi_def_bitmap_set_opaque(_bitmap: *mut c_void, _opaque: bool) {}

unsafe extern "C" fn bi_def_bitmap_test_opaque(_bitmap: *mut c_void) -> bool {
    false
}

unsafe extern "C" fn bi_def_bitmap_get_buffer(bitmap: *mut c_void) -> *mut u8 {
    bitmap.cast()
}

unsafe extern "C" fn bi_def_bitmap_destroy(bitmap: *mut c_void) {
    // SAFETY: a bitmap from bi_def_bitmap_create.
    unsafe { bfree(bitmap) }
}

unsafe extern "C" fn bi_def_bitmap_modified(_bitmap: *mut c_void) {}

static BITMAP_CALLBACKS: NsgifBitmapCbVt = NsgifBitmapCbVt {
    create: Some(bi_def_bitmap_create),
    destroy: Some(bi_def_bitmap_destroy),
    get_buffer: Some(bi_def_bitmap_get_buffer),
    set_opaque: Some(bi_def_bitmap_set_opaque),
    test_opaque: Some(bi_def_bitmap_test_opaque),
    modified: Some(bi_def_bitmap_modified),
    get_rowspan: None,
};

/// `bzalloc`: `bmalloc` plus a zero of the block.
unsafe fn bzalloc(size: usize) -> *mut c_void {
    // SAFETY: a plain allocation.
    let mem = unsafe { bmalloc(size) };
    if !mem.is_null() {
        // SAFETY: `bmalloc` returned `size` bytes.
        unsafe { ptr::write_bytes(mem.cast::<u8>(), 0, size) };
    }
    mem
}

fn raw<T>(handle: Option<NonNull<T>>) -> *mut T {
    handle.map_or(ptr::null_mut(), NonNull::as_ptr)
}

/// The real libnsgif, libobs allocator and graphics calls.
pub struct GsHost;

impl Host for GsHost {
    type Texture = NonNull<c_void>;
    type Data = NonNull<u8>;
    type Cache = NonNull<*mut u8>;
    type Gif = NonNull<c_void>;
    type Bitmap = NonNull<u8>;
    type Internal = NonNull<Internal<GsHost>>;

    fn internal_new() -> Self::Internal {
        // SAFETY: one zeroed block the size of the state; bzalloc aligns to
        // 32 bytes and does not return null.
        let block = unsafe { bzalloc(size_of::<Internal<GsHost>>()) }.cast::<Internal<GsHost>>();
        // SAFETY: the block is fresh, writable and large enough.
        unsafe { block.write(Internal::default()) };
        NonNull::new(block).unwrap_or(NonNull::dangling())
    }
    fn internal(handle: &mut Self::Internal) -> &mut Internal<GsHost> {
        // SAFETY: a live block from internal_new, borrowed as long as the
        // image's handle is.
        unsafe { handle.as_mut() }
    }
    fn internal_free(handle: Self::Internal) {
        // SAFETY: a live block from internal_new; its fields are plain
        // handles with nothing to drop.
        unsafe { bfree(handle.as_ptr().cast()) }
    }

    fn gif_create() -> Option<Self::Gif> {
        let mut gif = ptr::null_mut();
        // SAFETY: a static callback table and an out pointer.
        unsafe { nsgif_create(&BITMAP_CALLBACKS, NSGIF_BITMAP_FMT_R8G8B8A8, &mut gif) };
        NonNull::new(gif)
    }
    fn read_file(path: &CStr) -> FileRead<Self::Data> {
        // SAFETY: a NUL-terminated path and mode.
        let file = unsafe { fopen_rb(path.as_ptr()) };
        if file.is_null() {
            return FileRead::OpenFailed;
        }
        // SAFETY: an open FILE, read whole into a block of its size, then
        // closed.
        unsafe {
            let stream = file.cast::<libc::FILE>();
            libc::fseek(stream, 0, libc::SEEK_END);
            let size = os_ftelli64(file) as usize;
            libc::fseek(stream, 0, libc::SEEK_SET);
            let data = bmalloc(size).cast::<u8>();
            let read = libc::fread(data.cast(), 1, size, stream);
            libc::fclose(stream);
            FileRead::Read {
                data: NonNull::new(data).unwrap_or(NonNull::dangling()),
                size,
                complete: read == size,
            }
        }
    }
    fn gif_scan(gif: Option<Self::Gif>, size: usize, data: Option<Self::Data>) -> u32 {
        // SAFETY: the nsgif object and the file's block of `size` bytes.
        unsafe { nsgif_data_scan(raw(gif), size, raw(data)) }
    }
    fn gif_complete(gif: Option<Self::Gif>) {
        // SAFETY: the nsgif object.
        unsafe { nsgif_data_complete(raw(gif)) }
    }
    fn gif_info(gif: Option<Self::Gif>) -> GifInfo {
        // SAFETY: the nsgif object; its info lives as long as it does and
        // is read here once.
        let info = unsafe { &*nsgif_get_info(raw(gif)) };
        GifInfo {
            width: info.width,
            height: info.height,
            frame_count: info.frame_count,
            loop_max: info.loop_max,
        }
    }
    fn gif_decode(gif: Option<Self::Gif>, frame: u32) -> (u32, Option<Self::Bitmap>) {
        let mut bitmap = ptr::null_mut();
        // SAFETY: the nsgif object and an out pointer.
        let result = unsafe { nsgif_frame_decode(raw(gif), frame, &mut bitmap) };
        (result, NonNull::new(bitmap.cast()))
    }
    fn gif_frame_delay(gif: Option<Self::Gif>, frame: i32) -> u32 {
        // SAFETY: the nsgif object; the frame is one it has, as in the C,
        // which reads the result without a check too.
        unsafe { (*nsgif_get_frame_info(raw(gif), frame as u32)).delay }
    }
    fn gif_destroy(gif: Option<Self::Gif>) {
        // SAFETY: the nsgif object, destroyed once.
        unsafe { nsgif_destroy(raw(gif)) }
    }
    fn with_bitmap(bitmap: Self::Bitmap, len: usize, f: &mut dyn FnMut(&mut [u8])) {
        // SAFETY: libnsgif's current frame, 4 * width * height bytes, not
        // touched by anything else during the call.
        f(unsafe { core::slice::from_raw_parts_mut(bitmap.as_ptr(), len) })
    }

    fn alloc_data(size: usize) -> Self::Data {
        // SAFETY: a plain allocation.
        NonNull::new(unsafe { bzalloc(size) }.cast()).unwrap_or(NonNull::dangling())
    }
    fn alloc_cache(count: u32) -> Self::Cache {
        // SAFETY: a plain allocation, zeroed: every pointer null.
        NonNull::new(unsafe { bzalloc(count as usize * size_of::<*mut u8>()) }.cast())
            .unwrap_or(NonNull::dangling())
    }
    fn free_data(data: Option<Self::Data>) {
        // SAFETY: null or a live block, freed once.
        unsafe { bfree(raw(data).cast()) }
    }
    fn free_cache(cache: Option<Self::Cache>) {
        // SAFETY: null or a live block, freed once.
        unsafe { bfree(raw(cache).cast()) }
    }
    fn cache_has(cache: Option<Self::Cache>, frame: i32) -> bool {
        // SAFETY: the frame cache of a loaded GIF and a frame it has.
        !unsafe { *raw(cache).offset(frame as isize) }.is_null()
    }
    fn cache_store(
        cache: Option<Self::Cache>,
        frame: i32,
        frame_data: Option<Self::Data>,
        offset: usize,
        bitmap: Option<Self::Bitmap>,
        len: usize,
    ) {
        // SAFETY: the frame cache and frame data of a loaded GIF; the frame
        // data holds every frame, so frame * len + len fits, and the bitmap
        // is libnsgif's current frame of len bytes.
        unsafe {
            let slot = raw(frame_data).add(offset);
            *raw(cache).offset(frame as isize) = slot;
            ptr::copy_nonoverlapping(raw(bitmap), slot, len);
        }
    }

    fn decode_file(
        path: &CStr,
        alpha_mode: ImageAlphaMode,
        format: &mut ColorFormat,
        cx: &mut u32,
        cy: &mut u32,
        space: &mut ColorSpace,
    ) -> Option<Self::Data> {
        // SAFETY: a NUL-terminated path and four out pointers.
        NonNull::new(unsafe {
            gs_create_texture_file_data3(path.as_ptr(), alpha_mode, format, cx, cy, space)
        })
    }
    fn texture_create_bitmap(
        cx: u32,
        cy: u32,
        format: ColorFormat,
        bitmap: Option<Self::Bitmap>,
    ) -> Option<Self::Texture> {
        let data: *const u8 = raw(bitmap);
        // SAFETY: one level, its data the current frame of cx * cy texels.
        NonNull::new(unsafe {
            gs_texture_create(cx, cy, format, 1, &data, crate::image_file::GS_DYNAMIC)
        })
    }
    fn texture_create_data(
        cx: u32,
        cy: u32,
        format: ColorFormat,
        data: Option<Self::Data>,
    ) -> Option<Self::Texture> {
        let data: *const u8 = raw(data);
        // SAFETY: one level, its data the decoded file.
        NonNull::new(unsafe { gs_texture_create(cx, cy, format, 1, &data, 0) })
    }
    fn texture_destroy(texture: Option<Self::Texture>) {
        // SAFETY: null or the image's texture.
        unsafe { gs_texture_destroy(raw(texture)) }
    }
    fn texture_set_image(
        texture: Option<Self::Texture>,
        cache: Option<Self::Cache>,
        frame: i32,
        linesize: u32,
    ) {
        // SAFETY: the image's texture and the cached frame (null if it
        // never decoded, which gs_texture_set_image checks).
        unsafe {
            let data = *raw(cache).offset(frame as isize);
            gs_texture_set_image(raw(texture), data, linesize, false);
        }
    }

    fn log(level: i32, message: &[u8]) {
        // On the stack, NULs dropped: no Rust heap here (see image_file.rs).
        let mut line = [0u8; 4097];
        let mut len = 0;
        for &b in message.iter().filter(|&&b| b != 0) {
            if let Some(slot) = line.get_mut(len).filter(|_| len < 4096) {
                *slot = b;
                len += 1;
            }
        }
        // SAFETY: one NUL-terminated string.
        unsafe { log_line(level, line.as_ptr().cast()) }
    }
}

/// The C `gs_image_file_ex_t`.
#[allow(non_camel_case_types)]
pub type gs_image_file_ex_t = ImageFileEx<GsHost>;

/// # Safety
/// Same contract as C `gs_image_file_ex_init`: null, or a writable image
/// (its old contents are ignored); `file` null or a NUL-terminated path.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_image_file_ex_init(
    image: *mut gs_image_file_ex_t,
    file: *const c_char,
    alpha_mode: ImageAlphaMode,
) {
    if image.is_null() {
        return;
    }
    // SAFETY: writable; zeroed (padding included, as the C memset does)
    // and written whole before it is borrowed.
    unsafe {
        ptr::write_bytes(image, 0, 1);
        image.write(ImageFileEx::default());
    }
    // SAFETY: null or a NUL-terminated path.
    let file = (!file.is_null()).then(|| unsafe { CStr::from_ptr(file) });
    // SAFETY: initialized above.
    let loaded = unsafe {
        (*image).init(file, alpha_mode);
        (*image).loaded
    };
    if !loaded {
        // SAFETY: the same image, all default after a failed init; zeroes
        // its padding too, as the C memset in free does.
        unsafe { ptr::write_bytes(image, 0, 1) };
    }
}

/// # Safety
/// Same contract as C `gs_image_file_ex_free`: null, or an image from init.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_image_file_ex_free(image: *mut gs_image_file_ex_t) {
    // SAFETY: null or a live image, borrowed for this call.
    if let Some(live) = unsafe { image.as_mut() } {
        live.free();
        // SAFETY: the same image; the C memset also zeroes its padding.
        unsafe { ptr::write_bytes(image, 0, 1) };
    }
}

/// # Safety
/// Same contract as C `gs_image_file_ex_init_texture`: a live image.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_image_file_ex_init_texture(image: *mut gs_image_file_ex_t) {
    // SAFETY: the caller passes a live image; null is not checked, as in
    // the C.
    unsafe { &mut *image }.init_texture();
}

/// # Safety
/// Same contract as C `gs_image_file_ex_tick`: a live image.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_image_file_ex_tick(
    image: *mut gs_image_file_ex_t,
    elapsed_time_ns: u64,
) -> bool {
    // SAFETY: the caller passes a live image; null is not checked, as in
    // the C.
    unsafe { &mut *image }.tick(elapsed_time_ns)
}

/// # Safety
/// Same contract as C `gs_image_file_ex_update_texture`: a live image.
#[unsafe(no_mangle)]
pub unsafe extern "C" fn gs_image_file_ex_update_texture(image: *mut gs_image_file_ex_t) {
    // SAFETY: the caller passes a live image; null is not checked, as in
    // the C.
    unsafe { &mut *image }.update_texture();
}
