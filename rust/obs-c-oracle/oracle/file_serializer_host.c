/* Symbols file-serializer.c needs from platform.c / bmem.c, without pulling
 * obs.h in. On Unix, os_fopen / os_fseeki64 / os_ftelli64 match platform.c.
 * os_unlink, os_rename, and os_file_exists are the Rust platform-nix shims.
 * os_safe_replace is oracle_host_os_safe_replace so the file oracles keep
 * the C copy. On Windows these follow platform.c (wide fopen) and
 * platform-windows.c (DeleteFileW / MoveFileExW). */
#define _FILE_OFFSET_BITS 64
#ifndef _WIN32
/* fseeko/ftello are POSIX; -std=c11 hides them without this. */
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

/* bmemdup comes from the Rust bmem port (obs-util). */

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* platform_conv_host.c: libobs platform.c's conversion over the real
 * util/utf8.c, whose utf8_to_wchar converts with flags 0 so invalid UTF-8
 * becomes U+FFFD instead of failing. */
size_t os_utf8_to_wcs_ptr(const char *str, size_t len, wchar_t **pstr);
void bfree(void *ptr);

/* platform.c os_fopen + os_wfopen (MSVC branch). Like libobs, the
 * conversion result is not checked. */
FILE *os_fopen(const char *path, const char *mode)
{
	wchar_t *wpath = NULL;
	wchar_t *wmode = NULL;
	FILE *file = NULL;

	if (path) {
		os_utf8_to_wcs_ptr(path, 0, &wpath);
		if (wpath) {
			os_utf8_to_wcs_ptr(mode, 0, &wmode);
			file = _wfopen(wpath, wmode);
			bfree(wmode);
		}
		bfree(wpath);
	}
	return file;
}

int os_fseeki64(FILE *file, int64_t offset, int origin)
{
	return _fseeki64(file, offset, origin);
}

int64_t os_ftelli64(FILE *file)
{
	return _ftelli64(file);
}

/* platform-windows.c os_unlink / os_rename / os_safe_replace. */
int os_unlink(const char *path)
{
	wchar_t *w_path;
	BOOL success;

	os_utf8_to_wcs_ptr(path, 0, &w_path);
	if (!w_path)
		return -1;
	success = DeleteFileW(w_path);
	bfree(w_path);
	return success ? 0 : -1;
}

int os_rename(const char *old_path, const char *new_path)
{
	wchar_t *old_path_utf16 = NULL;
	wchar_t *new_path_utf16 = NULL;
	int code = -1;

	if (!os_utf8_to_wcs_ptr(old_path, 0, &old_path_utf16))
		return -1;
	if (!os_utf8_to_wcs_ptr(new_path, 0, &new_path_utf16))
		goto error;
	code = MoveFileExW(old_path_utf16, new_path_utf16, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
error:
	bfree(old_path_utf16);
	bfree(new_path_utf16);
	return code;
}

int os_safe_replace(const char *target, const char *from, const char *backup)
{
	wchar_t *wtarget = NULL;
	wchar_t *wfrom = NULL;
	wchar_t *wbackup = NULL;
	int code = -1;

	if (!target || !from)
		return -1;
	if (!os_utf8_to_wcs_ptr(target, 0, &wtarget))
		return -1;
	if (!os_utf8_to_wcs_ptr(from, 0, &wfrom))
		goto fail;
	if (backup && !os_utf8_to_wcs_ptr(backup, 0, &wbackup))
		goto fail;
	if (ReplaceFileW(wtarget, wfrom, wbackup, 0, NULL, NULL))
		code = 0;
	else if (GetLastError() == ERROR_FILE_NOT_FOUND)
		code = MoveFileExW(wfrom, wtarget, MOVEFILE_REPLACE_EXISTING) ? 0 : -1;
fail:
	bfree(wtarget);
	bfree(wfrom);
	bfree(wbackup);
	return code;
}
#else
#include <unistd.h>

FILE *os_fopen(const char *path, const char *mode)
{
	return path ? fopen(path, mode) : NULL;
}

int os_fseeki64(FILE *file, int64_t offset, int origin)
{
	return fseeko(file, offset, origin);
}

int64_t os_ftelli64(FILE *file)
{
	return ftello(file);
}

/* os_unlink and os_rename are the Rust platform-nix shims. */

/* Private name: the Rust platform-nix shim owns os_safe_replace. The
 * file-serializer and config-file oracles call this copy. */
int oracle_host_os_safe_replace(const char *target, const char *from, const char *backup)
{
	if (backup && access(target, F_OK) == 0 && rename(target, backup) != 0)
		return -1;
	return rename(from, target);
}
#endif
