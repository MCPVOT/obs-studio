#include "plugins/obs-vst/build/obs-vst-linux.pch.hpp"

QStringList VstPlatformSearchDirs()
{
	QStringList dir_list;
#ifdef __linux__
	// If the user has set the VST_PATH environmental
	// variable, then use it. Else default to a list
	// of common locations.
	QString vstPathEnv(getenv("VST_PATH"));
	if (!vstPathEnv.isNull()) {
		dir_list.append(vstPathEnv.split(":"));
	} else {
		QString home(getenv("HOME"));
		// Choose the most common locations
		// clang-format off
		dir_list << "/usr/lib/vst/"
		         << "/usr/lib/lxvst/"
		         << "/usr/lib/linux_vst/"
		         << "/usr/lib64/vst/"
		         << "/usr/lib64/lxvst/"
		         << "/usr/lib64/linux_vst/"
		         << "/usr/local/lib/vst/"
		         << "/usr/local/lib/lxvst/"
		         << "/usr/local/lib/linux_vst/"
		         << "/usr/local/lib64/vst/"
		         << "/usr/local/lib64/lxvst/"
		         << "/usr/local/lib64/linux_vst/"
		         << home + "/.vst/"
		         << home + "/.lxvst/";
		// clang-format on
	}
#endif
	return dir_list;
}

QStringList VstPlatformFileFilters()
{
	QStringList filters;
#ifdef __linux__
	filters << "*.so"
		<< "*.o";
#endif
	return filters;
}

QString VstPlatformStripExtension(QString name)
{
#ifdef __linux__
	name.remove(".so", Qt::CaseInsensitive);
	name.remove(".o", Qt::CaseInsensitive);
#endif
	return name;
}

const char *VstPlatformMigratePath(obs_data_t *settings, const char *path)
{
#ifdef __linux__
	// Migrate freedesktop.org Flatpak runtime 21.08 VST paths to 22.08.
	if (QFile::exists("/.flatpak-info") && QString(path).startsWith("/app/extensions/Plugins/lxvst")) {
		QString newPath(path);
		newPath.replace("/app/extensions/Plugins/lxvst", "/app/extensions/Plugins/vst");
		obs_data_set_string(settings, "plugin_path", newPath.toStdString().c_str());
		path = obs_data_get_string(settings, "plugin_path");
	}
#endif
	return path;
}
