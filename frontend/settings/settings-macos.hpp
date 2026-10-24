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

/* Settings dialog features on macOS (#867): the vsync options, browser
 * hardware acceleration, the always-on-top projector update, the colon that
 * Finder rejects in filenames, and on Apple Silicon the OpenGL/Metal renderer
 * choice. No include guard: this file defines nothing elsewhere. */
#ifdef __APPLE__
#define OBS_SETTINGS_MACOS_VSYNC 1
#define OBS_SETTINGS_BROWSER_HWACCEL 1
#define OBS_SETTINGS_PROJECTOR_ON_TOP 1
#define OBS_SETTINGS_FILENAME_UNSAFE "[:]"
#ifdef __aarch64__
#define OBS_SETTINGS_RENDERER 1
#endif
#endif
