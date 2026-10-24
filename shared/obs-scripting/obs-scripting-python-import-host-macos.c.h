#include "shared/obs-scripting/build/obs-scripting-python-import-host-macos.pch.h"

/* The library inside the install folder's Python.framework. */
void obs_python_host_lib_name(struct dstr *out, int major, int minor)
{
	dstr_printf(out, "Python.framework/Versions/%d.%d/lib/libpython%d.%d.dylib", major, minor, major, minor);
}
