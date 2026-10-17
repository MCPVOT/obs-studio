#include "plugins/frontend-tools/build/frontend-tools-host-macos.pch.h"

void frontend_tools_host_load(void) {}

void frontend_tools_host_unload(void) {}

bool frontend_tools_host_python_ui(void)
{
	return true;
}

/* A path picked inside Python.framework is cut back to the folder that
 * holds the framework. */
size_t frontend_tools_host_python_path_len(const char *path)
{
	const char *framework = strstr(path, "/Python.framework/Versions");
	return framework ? (size_t)(framework - path) : strlen(path);
}
