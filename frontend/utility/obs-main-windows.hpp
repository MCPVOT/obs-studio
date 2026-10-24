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

/* Windows logging hooks for obs-main (#865): log lines go to an attached
 * debugger, and OBS_DEBUGBREAK_ON_ERROR builds break into it on errors.
 * No include guard: this file defines nothing off Windows, where obs-main
 * echoes log lines to the console instead. */
#ifdef _WIN32
#define OBS_MAIN_HOST_WINDOWS 1

static inline void obs_main_host_log_debugger(const char *str)
{
	if (!IsDebuggerPresent()) {
		return;
	}

	int wNum = MultiByteToWideChar(CP_UTF8, 0, str, -1, NULL, 0);
	if (wNum > 1) {
		static std::wstring wide_buf;
		static std::mutex wide_mutex;

		std::lock_guard<std::mutex> lock(wide_mutex);
		wide_buf.reserve(wNum + 1);
		wide_buf.resize(wNum - 1);
		MultiByteToWideChar(CP_UTF8, 0, str, -1, &wide_buf[0], wNum);
		wide_buf.push_back('\n');

		OutputDebugStringW(wide_buf.c_str());
	}
}

static inline void obs_main_host_log_break(int log_level)
{
#ifdef OBS_DEBUGBREAK_ON_ERROR
	if (log_level <= LOG_ERROR && IsDebuggerPresent()) {
		__debugbreak();
	}
#else
	(void)log_level;
#endif
}
#endif
