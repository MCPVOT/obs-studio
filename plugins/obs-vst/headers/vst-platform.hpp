#pragma once

#include <QString>
#include <QStringList>
#include <obs.h>

QStringList VstPlatformSearchDirs();
QStringList VstPlatformFileFilters();
QString VstPlatformStripExtension(QString name);
/* Preserve the input pointer unless migration updates and re-reads settings. */
const char *VstPlatformMigratePath(obs_data_t *settings, const char *path);
