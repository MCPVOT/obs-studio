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

/* Unix desktop (Linux and the BSDs) startup hooks for obs-main (#865).
 * No include guard: this file defines nothing elsewhere. */
#ifdef __unix__
#define OBS_MAIN_HOST_PRE_APP 1
static inline bool obs_main_host_pre_app()
{
	/* NOTE: Users blindly set this, but this theme is incompatible with Qt6 and
	 * crashes loading saved geometry. Just turn off this theme and let users complain OBS
	 * looks ugly instead of crashing. */
	const char *platform_theme = getenv("QT_QPA_PLATFORMTHEME");
	if (platform_theme && strcmp(platform_theme, "qt5ct") == 0) {
		unsetenv("QT_QPA_PLATFORMTHEME");
	}
	return true;
}
#endif
