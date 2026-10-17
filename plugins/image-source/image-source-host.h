#pragma once

/* The image source plugin's host interface (obs-rust/obs-studio#858): what
 * differs between Windows, macOS and the other POSIX hosts, so the shared
 * sources need no platform conditionals. One implementation per host,
 * image-source-host-<os>.c.h, picked by CMake (BUILD.md). */

#include <stdbool.h>
#include <stdint.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Whether the host decodes JPEG XR (.jxr) images (WIC on Windows only). */
bool image_source_host_jxr_supported(void);

/* The Image source's file dialog filter. */
const char *image_source_host_image_filter(void);

/* The slideshows' file dialog filter. */
const char *image_source_host_slideshow_filter(void);

/* Sub-second part of a stat() modification time. POSIX hosts only: Windows
 * reads the file's last write time instead (image-file-version-win32.c.h). */
int64_t image_source_host_stat_mtime_nsec(const struct stat *stats);

#ifdef __cplusplus
}
#endif
