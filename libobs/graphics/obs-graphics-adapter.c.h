#include "libobs/build/libobs.pch.h"

/* Private adapters for the Rust obs-graphics exports (gs_image_file_ex_*,
 * gs_texrender_*), not a new public API. obs-graphics calls the libobs
 * allocator, log and file helpers only through these, so an executable that links the Rust staticlib next to
 * libobs (test_effect_parser_lifecycle_rust) never pulls the staticlib's own
 * copies of them, which clash with libobs's exports on Windows. */
void *obs_graphics_bmalloc(size_t size);
void obs_graphics_bfree(void *ptr);
void obs_graphics_log(int level, const char *line);
FILE *obs_graphics_fopen_rb(const char *path);
int64_t obs_graphics_ftelli64(FILE *file);

void *obs_graphics_bmalloc(size_t size)
{
	return bmalloc(size);
}

void obs_graphics_bfree(void *ptr)
{
	bfree(ptr);
}

void obs_graphics_log(int level, const char *line)
{
	blog(level, "%s", line);
}

FILE *obs_graphics_fopen_rb(const char *path)
{
	return os_fopen(path, "rb");
}

int64_t obs_graphics_ftelli64(FILE *file)
{
	return os_ftelli64(file);
}
