#include "plugins/image-source/build/image-source.pch.h"

/* image_file_version_get(): image-source-host-<os>.c.h on POSIX hosts,
 * image-file-version-win32.c.h on Windows. */

bool image_file_version_equal(const struct image_file_version *a, const struct image_file_version *b)
{
	return a->exists == b->exists && a->mtime_sec == b->mtime_sec && a->mtime_nsec == b->mtime_nsec &&
	       a->size == b->size;
}

/* Seconds between 1601-01-01 and 1970-01-01. */
#define FILETIME_UNIX_EPOCH 11644473600LL

void image_file_version_from_filetime(struct image_file_version *version, uint64_t filetime, uint64_t size)
{
	memset(version, 0, sizeof(*version));
	version->exists = true;
	version->mtime_sec = (int64_t)(filetime / 10000000ULL) - FILETIME_UNIX_EPOCH;
	version->mtime_nsec = (int64_t)(filetime % 10000000ULL) * 100;
	version->size = (int64_t)size;
}

/* File-local macros (BUILD.md): out of the next file in the unity group. */
#undef FILETIME_UNIX_EPOCH
