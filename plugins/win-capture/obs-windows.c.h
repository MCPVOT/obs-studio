#include "plugins/win-capture/build/win-capture.pch.h"

/* OBS's own windows for Window Capture (#2226). ms_fill_window_list and
 * ms_find_window leave out the windows of the current process, so the
 * "list OBS windows" option lists and finds them here instead. */

struct obs_windows_find {
	const char *window_class;
	const char *title;
	const char *exe;
	enum window_priority priority;
	HWND best;
	int best_rating;
};

static void obs_windows_encode(struct dstr *str)
{
	dstr_replace(str, "#", "#22");
	dstr_replace(str, ":", "#3A");
}

/* The executable's file name, as ms_get_window_exe gives it for other processes. */
static void obs_windows_exe(struct dstr *exe)
{
	wchar_t path[MAX_PATH];
	DWORD len = GetModuleFileNameW(NULL, path, MAX_PATH);
	if (!len || len == MAX_PATH) {
		dstr_copy(exe, "obs64.exe");
		return;
	}

	const wchar_t *slash = wcsrchr(path, L'\\');
	dstr_from_wcs(exe, slash ? slash + 1 : path);
}

static bool obs_windows_valid(HWND window, enum window_search_mode mode)
{
	DWORD id = 0;
	GetWindowThreadProcessId(window, &id);
	if (id != GetCurrentProcessId())
		return false;
	if (!IsWindowVisible(window) || (mode == EXCLUDE_MINIMIZED && IsIconic(window)))
		return false;

	DWORD styles = (DWORD)GetWindowLongPtr(window, GWL_STYLE);
	DWORD ex_styles = (DWORD)GetWindowLongPtr(window, GWL_EXSTYLE);
	if ((ex_styles & WS_EX_TOOLWINDOW) || (styles & WS_CHILD))
		return false;

	RECT rect;
	GetClientRect(window, &rect);
	return mode != EXCLUDE_MINIMIZED || (rect.bottom != 0 && rect.right != 0);
}

static BOOL CALLBACK obs_windows_add(HWND window, LPARAM param)
{
	obs_property_t *p = (obs_property_t *)param;

	if (!obs_windows_valid(window, EXCLUDE_MINIMIZED))
		return TRUE;

	struct dstr title = {0}, window_class = {0}, exe = {0}, desc = {0}, encoded = {0};
	ms_get_window_title(&title, window);
	ms_get_window_class(&window_class, window);
	obs_windows_exe(&exe);

	dstr_printf(&desc, "[%s]: %s", exe.array, title.array ? title.array : "");

	obs_windows_encode(&title);
	obs_windows_encode(&window_class);
	obs_windows_encode(&exe);
	dstr_printf(&encoded, "%s:%s:%s", title.array ? title.array : "", window_class.array ? window_class.array : "",
		    exe.array);

	obs_property_list_add_string(p, desc.array, encoded.array);

	dstr_free(&encoded);
	dstr_free(&desc);
	dstr_free(&exe);
	dstr_free(&window_class);
	dstr_free(&title);
	return TRUE;
}

/* Add OBS's own windows to a window list, in ms_fill_window_list's format. */
static void obs_windows_fill(obs_property_t *p)
{
	EnumWindows(obs_windows_add, (LPARAM)p);
}

static int obs_windows_rating(struct obs_windows_find *find, HWND window)
{
	struct dstr title = {0}, window_class = {0};
	ms_get_window_title(&title, window);
	ms_get_window_class(&window_class, window);

	const bool class_matches = window_class.array && find->window_class &&
				   strcmp(window_class.array, find->window_class) == 0;
	const bool title_matches = title.array && find->title && strcmp(title.array, find->title) == 0;
	int rating = 0;

	if (find->priority == WINDOW_PRIORITY_TITLE)
		rating = title_matches ? (class_matches ? 3 : 2) : (class_matches ? 1 : 0);
	else
		rating = class_matches ? (title_matches ? 3 : 2) : (title_matches ? 1 : 0);

	dstr_free(&window_class);
	dstr_free(&title);
	return rating;
}

static BOOL CALLBACK obs_windows_match(HWND window, LPARAM param)
{
	struct obs_windows_find *find = (struct obs_windows_find *)param;

	if (!obs_windows_valid(window, INCLUDE_MINIMIZED))
		return TRUE;

	int rating = obs_windows_rating(find, window);
	if (rating > find->best_rating) {
		find->best = window;
		find->best_rating = rating;
	}
	return TRUE;
}

/* The OBS window that matches a saved window, or NULL when exe is not OBS's. */
static HWND obs_windows_find(enum window_priority priority, const char *window_class, const char *title,
			     const char *exe)
{
	struct dstr own_exe = {0};
	obs_windows_exe(&own_exe);
	const bool own = exe && own_exe.array && astrcmpi(exe, own_exe.array) == 0;
	dstr_free(&own_exe);
	if (!own)
		return NULL;

	struct obs_windows_find find = {
		.window_class = window_class,
		.title = title,
		.exe = exe,
		.priority = priority,
	};
	EnumWindows(obs_windows_match, (LPARAM)&find);
	/* A window that matches neither its class nor its title is another OBS window. */
	return find.best_rating > 0 ? find.best : NULL;
}
