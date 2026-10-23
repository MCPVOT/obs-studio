#include "shared/obs-scripting/build/obs-scripting-lua-host-macos.pch.h"

/* Lua C modules in the app bundle's PlugIns folder, when it has one. */
void obs_lua_host_module_dirs(struct dstr *cpath, obs_lua_cpath_add_t add)
{
	struct dstr bundle_path;

	dstr_init_move_array(&bundle_path, os_get_executable_path_ptr(""));
	dstr_cat(&bundle_path, "../PlugIns");
	char *absolute_plugin_path = os_get_abs_path_ptr(bundle_path.array);
	dstr_free(&bundle_path);

	if (absolute_plugin_path)
		add(cpath, absolute_plugin_path);
	bfree(absolute_plugin_path);
}
