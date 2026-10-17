#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "plugins/image-source/image-source-host-posix.c.h"

/*
 * image-source's host interface (obs-rust/obs-studio#858): on POSIX hosts
 * other than macOS there is no JPEG XR decoder, so the file dialogs offer no
 * *.jxr, and the sub-second modification time comes from st_mtim. The filter
 * strings are pinned byte for byte as they were before the move.
 */

static void test_no_jxr(void **state)
{
	(void)state;
	assert_false(image_source_host_jxr_supported());
}

static void test_image_filter(void **state)
{
	(void)state;
	assert_string_equal(image_source_host_image_filter(),
			    "All formats (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.psd *.webp);;"
			    "BMP Files (*.bmp);;"
			    "Targa Files (*.tga);;"
			    "PNG Files (*.png);;"
			    "JPEG Files (*.jpeg *.jpg);;"
			    "GIF Files (*.gif);;"
			    "PSD Files (*.psd);;"
			    "WebP Files (*.webp);;"
			    "All Files (*.*)");
}

static void test_slideshow_filter(void **state)
{
	(void)state;
	assert_string_equal(image_source_host_slideshow_filter(),
			    "Image files (*.bmp *.tga *.png *.jpeg *.jpg *.gif *.webp)");
}

static void test_stat_mtime_nsec(void **state)
{
	(void)state;
	struct stat stats;

	memset(&stats, 0, sizeof(stats));
	stats.st_mtim.tv_sec = 1700000000;
	stats.st_mtim.tv_nsec = 123456789;
	assert_int_equal(image_source_host_stat_mtime_nsec(&stats), 123456789);

	stats.st_mtim.tv_nsec = 0;
	assert_int_equal(image_source_host_stat_mtime_nsec(&stats), 0);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_no_jxr),
		cmocka_unit_test(test_image_filter),
		cmocka_unit_test(test_slideshow_filter),
		cmocka_unit_test(test_stat_mtime_nsec),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
