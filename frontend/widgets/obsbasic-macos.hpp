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

/* Main window features on macOS (#871): Return/Backspace shortcuts and app
 * menu roles, Qt's own quit and deferred deletes, colorspace handling, the
 * system fullscreen menu, the permissions menu, the updater, the crash-log
 * menu, masked tray icons and hybrid MOV recordings. No include guard: this
 * file defines nothing elsewhere. */
#ifdef __APPLE__
#define OBS_BASIC_MACOS 1
#define OBS_BASIC_UPDATER 1
#define OBS_BASIC_CRASH_LOGS 1

#define DEFAULT_CONTAINER "hybrid_mov"

#define OBS_BASIC_TRAY_ACTIVE ":/res/images/tray_active_macos.svg"
#define OBS_BASIC_TRAY_ACTIVE_THEME "obs-tray"
#define OBS_BASIC_TRAY_IDLE ":/res/images/obs_macos.svg"
#define OBS_BASIC_TRAY_PAUSED ":/res/images/obs_paused_macos.svg"
#define OBS_BASIC_TRAY_MASK true
#endif
