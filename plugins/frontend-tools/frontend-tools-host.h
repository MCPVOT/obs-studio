#pragma once

/* The frontend tools' host interface (obs-rust/obs-studio#855): what differs
 * between Windows, macOS and the other POSIX hosts, so the shared sources need
 * no platform conditionals. Load/unload and the Python settings live in
 * frontend-tools-host-<os>.c.h; the scene switcher check lives next to each
 * host's window lookup (auto-scene-switcher-{win,osx,nix}). CMake picks one
 * host (BUILD.md). */

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Host-only tools to start and stop with the module (captions on Windows). */
void frontend_tools_host_load(void);
void frontend_tools_host_unload(void);

/* Whether the automatic scene switcher can see windows on this host (not
 * under Wayland). */
bool frontend_tools_host_scene_switcher_supported(void);

/* Whether the Scripts dialog offers the Python install path settings. */
bool frontend_tools_host_python_ui(void);

/* How many leading bytes of a saved Python install path to keep: macOS cuts
 * it before "/Python.framework/Versions"; other hosts keep all of it. */
size_t frontend_tools_host_python_path_len(const char *path);

#ifdef __cplusplus
}
#endif
