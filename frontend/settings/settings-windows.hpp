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

/* Settings dialog features on Windows (#867): the Windows-only advanced
 * options (audio ducking, hiding OBS from capture, process priority, the new
 * socket loop and low-latency mode), the Direct3D/OpenGL renderer choice,
 * browser hardware acceleration, the always-on-top projector update and the
 * filename characters Windows rejects. No include guard: this file defines
 * nothing elsewhere. */
#ifdef _WIN32
#define OBS_SETTINGS_WINDOWS 1
#define OBS_SETTINGS_RENDERER 1
#define OBS_SETTINGS_BROWSER_HWACCEL 1
#define OBS_SETTINGS_PROJECTOR_ON_TOP 1
#define OBS_SETTINGS_FILENAME_UNSAFE "[<>:\"\\|\\?\\*]"
#endif
