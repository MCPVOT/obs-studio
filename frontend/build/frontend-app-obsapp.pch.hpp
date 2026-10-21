/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.app-obsapp. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* OBSApp.cpp */
#include "frontend/OBSApp.hpp"
#include <utility/GpuStatus.hpp>

#include <utility/output-filename-format.h>
#include <utility/output-filename-reserve.h>

#include <components/Multiview.hpp>
#include <dialogs/LogUploadDialog.hpp>
#include <plugin-manager/PluginManager.hpp>
#include <utility/CrashHandler.hpp>
#include <utility/ConfigMigration.hpp>
#include <utility/OBSEventFilter.hpp>
#include <utility/OBSProxyStyle.hpp>
#include <utility/product-name.h>
#include <utility/platform.hpp>
#include <utility/scene-collection-migration.hpp>
#ifdef OBS_PLATFORM_HAS_UPDATE_BRANCHES
#include <utility/models/branches.hpp>
#endif
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>
#include <utility/settings-migration.hpp>
#include <utility/unclean-launch.hpp>

#include <QCheckBox>
#include <QDesktopServices>
#include <QLockFile>
#ifdef OBS_PLATFORM_HAS_UPDATE_BRANCHES
#include <QFile>
#endif

#include <QSessionManager>

#include <chrono>
#include <sstream>
