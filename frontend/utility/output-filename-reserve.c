#include "output-filename-reserve.h"

#include <string.h>

#include <util/bmem.h>
#include <util/dstr.h>
#include <util/platform.h>

/* Stub: the check-then-use logic of FindBestFilename() in OBSApp.cpp. */
char *output_filename_reserve(const char *path, bool no_space, bool overwrite, bool *reserved)
{
	if (reserved)
		*reserved = false;
	if (!path)
		return NULL;
	if (overwrite || !os_file_exists(path))
		return bstrdup(path);

	const char *ext = strrchr(path, '.');
	if (!ext)
		return bstrdup(path);

	size_t ext_start = (size_t)(ext - path);
	struct dstr test = {0};
	for (int num = 2;; num++) {
		dstr_ncopy(&test, path, ext_start);
		dstr_catf(&test, no_space ? "_%d" : " (%d)", num);
		dstr_cat(&test, ext);
		if (!os_file_exists(test.array))
			return test.array;
	}
}

void output_filename_release(const char *path)
{
	(void)path;
}
