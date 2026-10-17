#include "plugins/vlc-video/build/vlc-video-windows.pch.h"

void *vlc_host_open_libvlc(void)
{
	void *module = NULL;
	char *path_utf8 = NULL;
	wchar_t path[1024];
	LSTATUS status;
	DWORD size;
	HKEY key;

	memset(path, 0, 1024 * sizeof(wchar_t));

	status = RegOpenKeyW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\VideoLAN\\VLC", &key);
	if (status != ERROR_SUCCESS)
		return NULL;

	size = 1024;
	status = RegQueryValueExW(key, L"InstallDir", NULL, NULL, (LPBYTE)path, &size);
	if (status == ERROR_SUCCESS) {
		wcscat(path, L"\\libvlc.dll");
		os_wcs_to_utf8_ptr(path, 0, &path_utf8);
		module = os_dlopen(path_utf8);
		bfree(path_utf8);
	}

	RegCloseKey(key);
	return module;
}

void vlc_host_unload(void) {}

void vlc_host_native_path(struct dstr *path, bool is_url)
{
	if (!is_url)
		dstr_replace(path, "/", "\\");
}
