/*
 * Copyright (c) 2023 Lain Bailey <lain@obsproject.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "build/libobs-ported-windows-platform.pch.h"

/* The Windows bodies of platform-file.c.h's file functions (#874): UTF-8
 * paths become wide paths for the MSVC CRT's wide calls. MinGW's CRT has the
 * same calls. */

FILE *os_wfopen(const wchar_t *path, const char *mode)
{
	FILE *file = NULL;

	if (path) {
		wchar_t *wcs_mode;

		os_utf8_to_wcs_ptr(mode, 0, &wcs_mode);
		file = _wfopen(path, wcs_mode);
		bfree(wcs_mode);
	}

	return file;
}

FILE *os_fopen(const char *path, const char *mode)
{
	wchar_t *wpath = NULL;
	FILE *file = NULL;

	if (path) {
		os_utf8_to_wcs_ptr(path, 0, &wpath);
		file = os_wfopen(wpath, mode);
		bfree(wpath);
	}

	return file;
}

int os_stat(const char *file, struct stat *st)
{
	if (file) {
		wchar_t w_file[512];
		size_t size = os_utf8_to_wcs(file, 0, w_file, sizeof(w_file));
		if (size > 0) {
			struct _stat st_w32;
			int ret = _wstat(w_file, &st_w32);
			if (ret == 0) {
				st->st_dev = st_w32.st_dev;
				st->st_ino = st_w32.st_ino;
				st->st_mode = st_w32.st_mode;
				st->st_nlink = st_w32.st_nlink;
				st->st_uid = st_w32.st_uid;
				st->st_gid = st_w32.st_gid;
				st->st_rdev = st_w32.st_rdev;
				st->st_size = st_w32.st_size;
				st->st_atime = st_w32.st_atime;
				st->st_mtime = st_w32.st_mtime;
				st->st_ctime = st_w32.st_ctime;
			}

			return ret;
		}
	}

	return -1;
}

int os_fseeki64(FILE *file, int64_t offset, int origin)
{
	return _fseeki64(file, offset, origin);
}

int64_t os_ftelli64(FILE *file)
{
	return _ftelli64(file);
}
