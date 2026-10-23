#include "shared/obs-scripting/build/obs-scripting-lua-host-posix.pch.h"

/* Lua C modules next to the working directory, then in the script plugin
 * folder relative to the executable's folder. */
void obs_lua_host_module_dirs(struct dstr *cpath, obs_lua_cpath_add_t add)
{
	add(cpath, "./");

	char *relative_script_path = os_get_executable_path_ptr("../" SCRIPT_DIR);
	if (relative_script_path)
		add(cpath, relative_script_path);
	bfree(relative_script_path);
}
