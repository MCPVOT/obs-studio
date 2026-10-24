/* The image loader's host interface (obs-rust/obs-studio#872): a second
 * decoder for files FFmpeg cannot load. */

#pragma once

#include <stdint.h>
#include "graphics/graphics.h"

/* The decoded pixels (bmalloc'd), or NULL; the outputs are set only on
 * success. */
uint8_t *gs_image_host_fallback_load(const char *file, enum gs_color_format *format, uint32_t *cx_out, uint32_t *cy_out,
				     enum gs_color_space *space);
