#pragma once

/* The Lua scripting host interface (obs-rust/obs-studio#882): what differs
 * between Windows, macOS and the other POSIX hosts when Lua looks for C
 * modules, so obs-scripting-lua.c needs no platform conditionals. Each host
 * lives in obs-scripting-lua-host-<os>.c; CMake picks one (BUILD.md). The
 * module suffix is OBS_LUA_SO_EXT, a string literal CMake defines per host
 * ("dll" or "so"), because the startup scripts splice it in at compile time. */

#include <util/dstr.h>

/* Appends one directory to a package.cpath assignment. */
typedef void (*obs_lua_cpath_add_t)(struct dstr *cpath, const char *dir);

/* Adds, through add, the directories this host searches for Lua C modules
 * before SCRIPT_DIR, in search order. */
void obs_lua_host_module_dirs(struct dstr *cpath, obs_lua_cpath_add_t add);
