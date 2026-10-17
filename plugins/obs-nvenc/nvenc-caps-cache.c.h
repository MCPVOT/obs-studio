#include "build/obs-nvenc-caps-cache.pch.h"

/* RED: stubs, replaced by the implementation in the next commit. */

char *nvenc_caps_cache_lookup(const char *file_data, const char *key)
{
	UNUSED_PARAMETER(file_data);
	UNUSED_PARAMETER(key);
	return NULL;
}

char *nvenc_caps_cache_format(const char *key, const char *caps)
{
	UNUSED_PARAMETER(key);
	UNUSED_PARAMETER(caps);
	return NULL;
}
