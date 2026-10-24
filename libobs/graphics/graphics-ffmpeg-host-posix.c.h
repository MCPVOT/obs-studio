#include "libobs/build/libobs-graphics-ffmpeg-host-posix.pch.h"

/* The image loader's macOS and Linux host (obs-rust/obs-studio#872): no
 * second decoder behind FFmpeg. */

uint8_t *gs_image_host_fallback_load(const char *file, enum gs_color_format *format, uint32_t *cx_out, uint32_t *cy_out,
				     enum gs_color_space *space)
{
	(void)file;
	(void)format;
	(void)cx_out;
	(void)cy_out;
	(void)space;
	return NULL;
}
