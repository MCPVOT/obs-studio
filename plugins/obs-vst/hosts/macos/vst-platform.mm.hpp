#include "plugins/obs-vst/build/obs-vst-macos.pch.hpp"

QStringList VstPlatformSearchDirs()
{
	QStringList dir_list;
	dir_list << "/Library/Audio/Plug-Ins/VST/"
		 << "~/Library/Audio/Plug-ins/VST/";
	return dir_list;
}

QStringList VstPlatformFileFilters()
{
	QStringList filters;
	filters << "*.vst";
	return filters;
}

QString VstPlatformStripExtension(QString name)
{
	name.remove(".vst", Qt::CaseInsensitive);
	return name;
}

const char *VstPlatformMigratePath(obs_data_t *settings, const char *path)
{
	(void)settings;
	return path;
}
