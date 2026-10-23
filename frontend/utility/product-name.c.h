#include "frontend/build/frontend-utility-product-name.pch.h"

int obs_product_string(char *out, size_t size, const char *obs_version, const char *rust_version)
{
	if (rust_version) {
		return snprintf(out, size, "OBS-Studio-Rust %s (based on OBS %s)", rust_version, obs_version);
	}
	return snprintf(out, size, "OBS %s", obs_version);
}

#define RUST_TAG_PREFIX "obs-studio-rust-v"

/* Splits "obs-studio-rust-v<ver>-<n>-g<sha>" into its parts. False for
 * anything else. */
static bool parse_rust_describe(const char *describe, const char **ver, size_t *ver_len, const char **n, size_t *n_len,
				const char **sha)
{
	const size_t prefix_len = sizeof(RUST_TAG_PREFIX) - 1;

	if (!describe || strncmp(describe, RUST_TAG_PREFIX, prefix_len) != 0) {
		return false;
	}
	const char *rest = describe + prefix_len;
	const char *g = strrchr(rest, '-');
	if (!g || g[1] != 'g' || !g[2]) {
		return false;
	}

	/* The commit count is the digits between the previous '-' and g. */
	const char *count = g;
	while (count > rest && count[-1] >= '0' && count[-1] <= '9') {
		count--;
	}
	if (count == g || count <= rest + 1 || count[-1] != '-') {
		return false;
	}

	*ver = rest;
	*ver_len = (size_t)(count - 1 - rest);
	*n = count;
	*n_len = (size_t)(g - count);
	*sha = g + 1;
	return true;
}

int obs_product_title(char *out, size_t size, const char *release_version, const char *git_describe)
{
	const char *ver, *n, *sha;
	size_t ver_len, n_len;

	if (release_version && *release_version) {
		return snprintf(out, size, "OBS Studio Rust %s", release_version);
	}
	if (parse_rust_describe(git_describe, &ver, &ver_len, &n, &n_len, &sha)) {
		return snprintf(out, size, "OBS Studio Rust %.*s-dev+%.*s-%s", (int)ver_len, ver, (int)n_len, n, sha);
	}
	return snprintf(out, size, "OBS Studio Rust dev");
}

#undef RUST_TAG_PREFIX
