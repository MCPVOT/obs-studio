#include "plugins/frontend-tools/build/frontend-tools-host-windows.pch.h"

/* captions.cpp: the SAPI captions tool, Windows only. */
void InitCaptions(void);
void FreeCaptions(void);

void frontend_tools_host_load(void)
{
	InitCaptions();
}

void frontend_tools_host_unload(void)
{
	FreeCaptions();
}

bool frontend_tools_host_python_ui(void)
{
	return true;
}

size_t frontend_tools_host_python_path_len(const char *path)
{
	return strlen(path);
}
