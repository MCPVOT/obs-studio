/* Display product name for the window title, About dialog, --version and
 * log/crash uploads. Plain C so a cmocka test can build it without Qt. */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Writes "OBS <obs_version>", or for the Rust build (rust_version not NULL)
 * "OBS-Studio-Rust <rust_version> (based on OBS <obs_version>)", into out.
 * Returns the full length like snprintf; out is always NUL-terminated when
 * size > 0. */
int obs_product_string(char *out, size_t size, const char *obs_version, const char *rust_version);

/* Writes the main window's product title, "OBS Studio Rust <version>", into
 * out (#1983). <version> is release_version when the build is a release (not
 * NULL or empty). Otherwise it comes from git_describe, the configure-time
 * `git describe --tags --long --match 'obs-studio-rust-v*'` output:
 * "obs-studio-rust-v<ver>-<n>-g<sha>" gives "<ver>-dev+<n>-g<sha>"; anything
 * else (NULL, empty, no tag) gives "dev". Returns the full length like
 * snprintf; out is always NUL-terminated when size > 0. */
int obs_product_title(char *out, size_t size, const char *release_version, const char *git_describe);

#ifdef __cplusplus
}
#endif
