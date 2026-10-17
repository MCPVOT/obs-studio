/* Host interface of the VLC video source (obs-rust/obs-studio#857). Each host
 * implements it in vlc-video-host-<os>.c.h, built in its own unity group that
 * CMakeLists.txt picks per OS; the shared files call only these. */

#pragma once

#include <stdbool.h>

struct dstr;

/* Finds and opens the libVLC library: the VLC install directory from the
 * registry (Windows), /Applications/VLC.app with libvlccore and
 * VLC_PLUGIN_PATH set up first (macOS), libvlc.so.5 elsewhere. Returns the
 * os_dlopen handle, or NULL when VLC is not installed. */
void *vlc_host_open_libvlc(void);

/* Releases what vlc_host_open_libvlc() loaded besides the handle it
 * returned (macOS: libvlccore). Called before that handle is closed. */
void vlc_host_unload(void);

/* Turns a playlist path into the host's native form in place: on Windows a
 * local file path gets backslashes. URLs are left alone everywhere. */
void vlc_host_native_path(struct dstr *path, bool is_url);
