#include "plugins/image-source/build/image-source-macos.pch.h"

bool image_source_host_jxr_supported(void)
{
	return false;
}

const char *image_source_host_image_filter(void)
{
	return "All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd *.webp);;"
	       "BMP Files (*.bmp);;"
	       "Targa Files (*.tga);;"
	       "PNG Files (*.png);;"
	       "JPEG Files (*.jpeg *.jpg);;"
	       "GIF Files (*.gif);;"
	       "PSD Files (*.psd);;"
	       "WebP Files (*.webp);;"
	       "All Files (*.*)";
}

const char *image_source_host_slideshow_filter(void)
{
	return "Image files (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.webp)";
}

int64_t image_source_host_stat_mtime_nsec(const struct stat *stats)
{
	return (int64_t)stats->st_mtimespec.tv_nsec;
}

void image_file_version_get(const char *path, struct image_file_version *version)
{
	struct stat stats;

	memset(version, 0, sizeof(*version));
	if (!path || os_stat(path, &stats) != 0)
		return;

	version->exists = true;
	version->mtime_sec = (int64_t)stats.st_mtime;
	version->mtime_nsec = image_source_host_stat_mtime_nsec(&stats);
	version->size = (int64_t)stats.st_size;
}
