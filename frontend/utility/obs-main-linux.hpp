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

/* Linux startup hooks for obs-main (#865): refuse to run inside a ChromeOS
 * Linux container. No include guard: this file defines nothing elsewhere. */
#ifdef __linux__
/* Portable-mode marker files sit one level up. */
#define BASE_PATH ".."

#define OBS_MAIN_HOST_REFUSE_LAUNCH 1
static inline bool obs_main_host_refuse_launch()
{
	// Mounted by termina during chromeOS linux container startup
	// https://chromium.googlesource.com/chromiumos/overlays/board-overlays/+/master/project-termina/chromeos-base/termina-lxd-scripts/files/lxd_setup.sh
	os_dir_t *crosDir = os_opendir("/opt/google/cros-containers");
	if (!crosDir) {
		return false;
	}

	QMessageBox::StandardButtons buttons(QMessageBox::Ok);
	QMessageBox mb(QMessageBox::Critical, QTStr("ChromeOS.Title"), QTStr("ChromeOS.Text"), buttons, nullptr);

	mb.exec();
	return true;
}
#endif
