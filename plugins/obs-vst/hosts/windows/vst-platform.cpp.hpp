#include "plugins/obs-vst/build/obs-vst-windows.pch.hpp"

QStringList VstPlatformSearchDirs()
{
	QStringList dir_list;
#ifndef _WIN64
	HANDLE hProcess = GetCurrentProcess();

	BOOL isWow64;
	IsWow64Process(hProcess, &isWow64);

	if (!isWow64) {
#endif
		dir_list << qEnvironmentVariable("ProgramFiles") + "/Steinberg/VstPlugins/"
			 << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/Shared Components/"
			 << qEnvironmentVariable("CommonProgramFiles") + "/VST2"
			 << qEnvironmentVariable("CommonProgramFiles") + "/Steinberg/VST2"
			 << qEnvironmentVariable("CommonProgramFiles") + "/VSTPlugins/"
			 << qEnvironmentVariable("ProgramFiles") + "/VSTPlugins/";
#ifndef _WIN64
	} else {
		dir_list << qEnvironmentVariable("ProgramFiles(x86)") + "/Steinberg/VstPlugins/"
			 << qEnvironmentVariable("CommonProgramFiles(x86)") + "/Steinberg/Shared Components/"
			 << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VST2"
			 << qEnvironmentVariable("CommonProgramFiles(x86)") + "/VSTPlugins/"
			 << qEnvironmentVariable("ProgramFiles(x86)") + "/VSTPlugins/";
	}
#endif
	return dir_list;
}

QStringList VstPlatformFileFilters()
{
	QStringList filters;
	filters << "*.dll";
	return filters;
}

QString VstPlatformStripExtension(QString name)
{
	name.remove(".dll", Qt::CaseInsensitive);
	return name;
}

const char *VstPlatformMigratePath(obs_data_t *settings, const char *path)
{
	(void)settings;
	return path;
}
