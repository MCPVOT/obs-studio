#include "plugins/coreaudio-encoder/build/coreaudio-encoder.pch.hpp"

bool ca_platform_load()
{
	if (!load_core_audio()) {
		CA_LOG(LOG_WARNING, "CoreAudio AAC encoder not installed on "
				    "the system or couldn't be loaded");
		return false;
	}
	CA_LOG(LOG_INFO, "Adding CoreAudio AAC encoder");
	return true;
}

void ca_platform_unload()
{
	unload_core_audio();
}

bool ca_platform_describe_status(OSStatus code, DStr &result)
{
	(void)code;
	(void)result;
	return false;
}

/* Preserve this optional module export on Windows only. */
void obs_module_unload(void)
{
	ca_platform_unload();
}
