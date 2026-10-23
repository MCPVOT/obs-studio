/* Test-only oracle: the original libobs/graphics/image-file.c.h,
 * unmodified, with its exports renamed to oracle_* so it can link next to
 * the Rust implementation. It calls the real libnsgif (built by build.rs)
 * and the recording gs_* stand-in in texture_render_device.c, the same ones
 * the Rust shim calls. */
#define gs_image_file_ex_init oracle_gs_image_file_ex_init
#define gs_image_file_ex_free oracle_gs_image_file_ex_free
#define gs_image_file_ex_init_texture oracle_gs_image_file_ex_init_texture
#define gs_image_file_ex_tick oracle_gs_image_file_ex_tick
#define gs_image_file_ex_update_texture oracle_gs_image_file_ex_update_texture

/* The original mixes int, uint32_t and size_t without the casts the
 * oracle's -Wsign-conversion wants. */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include "graphics/image-file.c.h"
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
/* The private C adapters the Rust shim calls for the allocator, log and
 * file helpers, copied verbatim by build.rs. */
#include "obs-graphics-adapter.inc"

size_t oracle_image_file_layout(size_t *out);

/* size, align, then the offset of every field of struct gs_image_file_ex in
 * declaration order. Returns how many values it wrote. */
size_t oracle_image_file_layout(size_t *out)
{
	size_t n = 0;

	out[n++] = sizeof(struct gs_image_file_ex);
	out[n++] = _Alignof(struct gs_image_file_ex);
	out[n++] = offsetof(struct gs_image_file_ex, texture);
	out[n++] = offsetof(struct gs_image_file_ex, format);
	out[n++] = offsetof(struct gs_image_file_ex, cx);
	out[n++] = offsetof(struct gs_image_file_ex, cy);
	out[n++] = offsetof(struct gs_image_file_ex, is_animated_gif);
	out[n++] = offsetof(struct gs_image_file_ex, frame_updated);
	out[n++] = offsetof(struct gs_image_file_ex, loaded);
	out[n++] = offsetof(struct gs_image_file_ex, internal);
	out[n++] = offsetof(struct gs_image_file_ex, cur_time);
	out[n++] = offsetof(struct gs_image_file_ex, cur_frame);
	out[n++] = offsetof(struct gs_image_file_ex, cur_loop);
	out[n++] = offsetof(struct gs_image_file_ex, texture_data);
	out[n++] = offsetof(struct gs_image_file_ex, mem_usage);
	out[n++] = offsetof(struct gs_image_file_ex, alpha_mode);
	out[n++] = offsetof(struct gs_image_file_ex, space);
	return n;
}
