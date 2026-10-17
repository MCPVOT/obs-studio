#include "plugins/image-source/build/image-source-windows.pch.h"

bool image_source_host_jxr_supported(void)
{
	return true;
}

const char *image_source_host_image_filter(void)
{
	return "All formats (*.bmp *.tga *.png *.jpeg *.jpg *.jxr *.gif *.psd *.webp);;"
	       "BMP Files (*.bmp);;"
	       "Targa Files (*.tga);;"
	       "PNG Files (*.png);;"
	       "JPEG Files (*.jpeg *.jpg);;"
	       "JXR Files (*.jxr);;"
	       "GIF Files (*.gif);;"
	       "PSD Files (*.psd);;"
	       "WebP Files (*.webp);;"
	       "All Files (*.*)";
}

const char *image_source_host_slideshow_filter(void)
{
	return "Image files (*.bmp *.tga *.png *.jpeg *.jpg *.jxr *.gif *.webp)";
}

/* The Windows image_file_version_get() reads the last write time
 * (image-file-version-win32.c.h), so stat() is not used for it. */
int64_t image_source_host_stat_mtime_nsec(const struct stat *stats)
{
	(void)stats;
	return 0;
}
