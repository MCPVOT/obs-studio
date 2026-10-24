#include "shared/obs-scripting/build/obs-scripting-python-host-macos.pch.h"

/* Python loads once the user picks its install folder. */
bool obs_python_host_load_at_start(void)
{
	return false;
}

/* PYTHONHOME is the version folder in the install folder's Python.framework. */
void obs_python_host_home(struct dstr *out, const char *python_path, int major, int minor)
{
	dstr_printf(out, "%s/Python.framework/Versions/%i.%i", python_path, major, minor);
}

/* No bytecode next to the scripts in the app bundle. */
bool obs_python_host_no_bytecode(void)
{
	return true;
}

static void add_bundle_dir(obs_python_path_add_t add, const char *bundle_dir)
{
	struct dstr path;

	dstr_init_move_array(&path, os_get_executable_path_ptr(""));
	dstr_cat(&path, bundle_dir);
	char *absolute_path = os_get_abs_path_ptr(path.array);
	dstr_free(&path);

	if (absolute_path)
		add(absolute_path);
	bfree(absolute_path);
}

/* The app bundle's PlugIns and Resources folders, when they exist. */
void obs_python_host_module_dirs(obs_python_path_add_t add)
{
	add_bundle_dir(add, "../PlugIns");
	add_bundle_dir(add, "../Resources");
}
