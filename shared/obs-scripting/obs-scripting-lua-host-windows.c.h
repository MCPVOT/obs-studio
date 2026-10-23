#include "shared/obs-scripting/build/obs-scripting-lua-host-windows.pch.h"

/* Lua C modules next to the working directory. */
void obs_lua_host_module_dirs(struct dstr *cpath, obs_lua_cpath_add_t add)
{
	add(cpath, "./");
}
