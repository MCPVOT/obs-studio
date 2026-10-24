#pragma once

/* The Python scripting host interface (obs-rust/obs-studio#882): what differs
 * between Windows, macOS and the other POSIX hosts when Python starts, so
 * obs-scripting.c and obs-scripting-python.c need no platform conditionals.
 * Each host lives in obs-scripting-python-host-<os>.c; CMake picks one
 * (BUILD.md). */

#include <stdbool.h>
#include <util/dstr.h>

/* Whether Python loads when scripting starts. Windows and macOS wait for the
 * Python install folder the user picks; other hosts link Python. */
bool obs_python_host_load_at_start(void);

/* Sets out to PYTHONHOME for Python major.minor installed in python_path. */
void obs_python_host_home(struct dstr *out, const char *python_path, int major, int minor);

/* Whether Python must not write bytecode (into the macOS app bundle). */
bool obs_python_host_no_bytecode(void);

/* Adds one directory to sys.path. */
typedef void (*obs_python_path_add_t)(const char *dir);

/* Adds, through add, the directories this host puts on sys.path so that
 * obspython imports, in order. */
void obs_python_host_module_dirs(obs_python_path_add_t add);
