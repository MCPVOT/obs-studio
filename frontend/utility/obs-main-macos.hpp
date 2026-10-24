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

/* macOS startup hooks for obs-main (#865): the NSApplication subclass and
 * thread locks, the app-bundle check, the application icon and the
 * permissions dialog. No include guard: this file defines nothing elsewhere. */
#ifdef __APPLE__
#define OBS_MAIN_HOST_MACOS 1

#define OBS_MAIN_HOST_PRE_APP 1
static inline bool obs_main_host_pre_app()
{
	InstallNSApplicationSubclass();
	InstallNSThreadLocks();

	if (!isInBundle()) {
		blog(LOG_ERROR,
		     "OBS cannot be run as a standalone binary on macOS. Run the Application bundle instead.");
		return false;
	}
	return true;
}

#define OBS_MAIN_HOST_VERIFY_STARTUP 1
static inline void obs_main_host_verify_startup()
{
	if (!VerifyMacApplicationIcon()) {
		throw "Failed to initialize the macOS application icon";
	}
}

#define OBS_MAIN_HOST_PERMISSIONS 1
static inline void obs_main_host_permissions()
{
	MacPermissionStatus audio_permission = CheckPermission(kAudioDeviceAccess);
	MacPermissionStatus video_permission = CheckPermission(kVideoDeviceAccess);
	MacPermissionStatus input_monitoring_permission = CheckPermission(kInputMonitoring);
	MacPermissionStatus screen_permission = CheckPermission(kScreenCapture);

	int permissionsDialogLastShown =
		config_get_int(App()->GetAppConfig(), "General", "MacOSPermissionsDialogLastShown");
	if (permissionsDialogLastShown < MACOS_PERMISSIONS_DIALOG_VERSION) {
		OBSPermissions check(nullptr, screen_permission, video_permission, audio_permission,
				     input_monitoring_permission);
		check.exec();
	}
}
#endif
