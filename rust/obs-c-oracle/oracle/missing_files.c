/* Test-only oracle: the original libobs/obs-missing-files.c.h, unmodified,
 * with its exports renamed to oracle_* so it can link next to the Rust
 * implementation. bmalloc and the two obs_source_* calls stay the real
 * symbols. Cargo tests provide the source calls. libobs provides them
 * when this file is not linked. */
#include <stddef.h>

int oracle_missing_file_enum_source(void);
int oracle_missing_file_enum_script(void);

#define obs_missing_files_create oracle_obs_missing_files_create
#define obs_missing_files_destroy oracle_obs_missing_files_destroy
#define obs_missing_files_add_file oracle_obs_missing_files_add_file
#define obs_missing_files_count oracle_obs_missing_files_count
#define obs_missing_files_get_file oracle_obs_missing_files_get_file
#define obs_missing_files_append oracle_obs_missing_files_append
#define obs_missing_file_create oracle_obs_missing_file_create
#define obs_missing_file_release oracle_obs_missing_file_release
#define obs_missing_file_destroy oracle_obs_missing_file_destroy
#define obs_missing_file_issue_callback oracle_obs_missing_file_issue_callback
#define obs_missing_file_get_path oracle_obs_missing_file_get_path
#define obs_missing_file_get_source_name oracle_obs_missing_file_get_source_name

/* libobs assigns an int to the long ref without the sign-conversion
 * warning the oracle enables. */
#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic ignored "-Wsign-conversion"
#endif

#include "obs-missing-files.c.h"

int oracle_missing_file_enum_source(void)
{
	return OBS_MISSING_FILE_SOURCE;
}

int oracle_missing_file_enum_script(void)
{
	return OBS_MISSING_FILE_SCRIPT;
}
