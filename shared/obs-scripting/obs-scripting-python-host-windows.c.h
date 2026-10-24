#include "shared/obs-scripting/build/obs-scripting-python-host-windows.pch.h"

/* Python loads once the user picks its install folder. */
bool obs_python_host_load_at_start(void)
{
	return false;
}

/* PYTHONHOME is the install folder itself. */
void obs_python_host_home(struct dstr *out, const char *python_path, int major, int minor)
{
	(void)major;
	(void)minor;
	dstr_copy(out, python_path);
}

bool obs_python_host_no_bytecode(void)
{
	return false;
}

/* The script plugin folder relative to the executable's folder, then
 * SCRIPT_DIR as an absolute path. */
void obs_python_host_module_dirs(obs_python_path_add_t add)
{
	char *relative_script_path = os_get_executable_path_ptr("../" SCRIPT_DIR);
	if (relative_script_path)
		add(relative_script_path);
	bfree(relative_script_path);

	char *absolute_script_path = os_get_abs_path_ptr(SCRIPT_DIR);
	if (absolute_script_path)
		add(absolute_script_path);
	bfree(absolute_script_path);
}
