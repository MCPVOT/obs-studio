#include "frontend/build/frontend-utility-product-name.pch.h"

int obs_product_string(char *out, size_t size, const char *obs_version, const char *rust_version)
{
	if (rust_version) {
		return snprintf(out, size, "OBS-Studio-Rust %s (based on OBS %s)", rust_version, obs_version);
	}
	return snprintf(out, size, "OBS %s", obs_version);
}
