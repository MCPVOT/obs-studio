#include "plugins/frontend-tools/build/frontend-tools-host-posix.pch.h"

void frontend_tools_host_load(void) {}

void frontend_tools_host_unload(void) {}

bool frontend_tools_host_python_ui(void)
{
	return false;
}

size_t frontend_tools_host_python_path_len(const char *path)
{
	return strlen(path);
}
