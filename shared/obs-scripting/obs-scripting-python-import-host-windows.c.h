#include "shared/obs-scripting/build/obs-scripting-python-import-host-windows.pch.h"

/* python312.dll, directly in the install folder. */
void obs_python_host_lib_name(struct dstr *out, int major, int minor)
{
	dstr_printf(out, "python%d%d.dll", major, minor);
}
