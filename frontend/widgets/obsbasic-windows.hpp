/******************************************************************************
    Copyright (C) 2023 by Lain Bailey <lain@obsproject.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/* Main window features on Windows (#871): the Qt6Network preload, the drop
 * style and early show, the Repair action, native window messages, the
 * updater, the crash-log menu and process priority while outputs run.
 * No include guard: this file defines nothing elsewhere. */
#ifdef _WIN32
#define OBS_BASIC_WINDOWS 1
#define OBS_BASIC_UPDATER 1
#define OBS_BASIC_CRASH_LOGS 1

#define OBS_BASIC_PROCESS_PRIORITY 1
static inline void UpdateProcessPriority()
{
	const char *priority = config_get_string(App()->GetAppConfig(), "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0) {
		SetProcessPriority(priority);
	}
}

static inline void ClearProcessPriority()
{
	const char *priority = config_get_string(App()->GetAppConfig(), "General", "ProcessPriority");
	if (priority && strcmp(priority, "Normal") != 0) {
		SetProcessPriority("Normal");
	}
}
#endif
