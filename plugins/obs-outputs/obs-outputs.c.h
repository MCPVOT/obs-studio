#include "plugins/obs-outputs/build/obs-outputs.pch.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-outputs", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "OBS core RTMP/FLV/null outputs";
}

extern struct obs_output_info rtmp_output_info;
extern struct obs_output_info null_output_info;
extern struct obs_output_info flv_output_info;
extern struct obs_output_info mp4_output_info;
extern struct obs_output_info mov_output_info;

bool obs_module_load(void)
{
	obs_outputs_host_load();

	obs_register_output(&rtmp_output_info);
	obs_register_output(&null_output_info);
	obs_register_output(&flv_output_info);
	obs_register_output(&mp4_output_info);
	obs_register_output(&mov_output_info);
	return true;
}

void obs_module_unload(void)
{
	obs_outputs_host_unload();
}
