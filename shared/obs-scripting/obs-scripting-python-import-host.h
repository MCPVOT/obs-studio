#pragma once

/* The Python import host interface (obs-rust/obs-studio#882): how Windows
 * and macOS name the Python library that obs-scripting-python-import.c loads
 * at run time, so it needs no platform conditionals. Each host lives in
 * obs-scripting-python-import-host-<os>.c; CMake picks one (BUILD.md). Other
 * hosts link Python and build no import code. */

#include <util/dstr.h>

/* Sets out to the path of Python major.minor's library, relative to the
 * install folder the user picked. */
void obs_python_host_lib_name(struct dstr *out, int major, int minor);
