#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Chooses the file a new recording (or screenshot, or stream dump) writes
 * to, and reserves it.
 *
 * path is the full path the filename format produced. With overwrite set,
 * path is returned unchanged and nothing is created. Otherwise the first of
 * path, "name (2).ext", "name (3).ext", ... ("name_2.ext", ... with no_space)
 * that does not exist yet is created as an empty file, atomically
 * (O_CREAT | O_EXCL), so two outputs that start in the same second, in one
 * OBS or in two, never get the same file. The output opens the file later
 * and truncates it.
 *
 * If the file cannot be created for any reason other than the name being
 * taken (a missing directory, no permission), the name is returned without
 * a reservation and the output reports the error when it opens the file, as
 * before. A path without an extension that already exists is returned as is.
 *
 * *reserved (may be NULL) tells whether this call created the file. Returns
 * a string the caller frees with bfree(), or NULL if path is NULL. */
char *output_filename_reserve(const char *path, bool no_space, bool overwrite, bool *reserved);

/* Removes a file reserved by output_filename_reserve() if it is still
 * empty, for an output that failed before writing anything. A file the
 * output has written to is kept. */
void output_filename_release(const char *path);

#ifdef __cplusplus
}
#endif
