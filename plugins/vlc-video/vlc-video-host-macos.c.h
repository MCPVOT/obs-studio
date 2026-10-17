#include "plugins/vlc-video/build/vlc-video-macos.pch.h"

/* According to otool -L, this is what libvlc.dylib wants. */
#define LIBVLC_DIR "/Applications/VLC.app/Contents/MacOS/"
#define LIBVLC_CORE_FILE LIBVLC_DIR "lib/libvlccore.dylib"
#define LIBVLC_FILE LIBVLC_DIR "lib/libvlc.5.dylib"

static void *libvlc_core_module = NULL;

void *vlc_host_open_libvlc(void)
{
	setenv("VLC_PLUGIN_PATH", LIBVLC_DIR "plugins", false);
	libvlc_core_module = os_dlopen(LIBVLC_CORE_FILE);

	if (!libvlc_core_module)
		return NULL;

	return os_dlopen(LIBVLC_FILE);
}

void vlc_host_unload(void)
{
	if (libvlc_core_module)
		os_dlclose(libvlc_core_module);
}

void vlc_host_native_path(struct dstr *path, bool is_url)
{
	UNUSED_PARAMETER(path);
	UNUSED_PARAMETER(is_url);
}

/* File-local macros (BUILD.md): out of the next file in the unity group. */
#undef LIBVLC_DIR
#undef LIBVLC_CORE_FILE
#undef LIBVLC_FILE
