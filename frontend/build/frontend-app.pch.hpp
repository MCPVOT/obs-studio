/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.app. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* OBSApp_Themes.cpp */
#include "frontend/OBSApp.hpp"

#include <utility/OBSProxyStyle.hpp>
#include <utility/OBSThemeVariable.hpp>
#include <utility/platform.hpp>

#include <qt-wrappers.hpp>
#include <ui-config.h>
#include <util/cf-parser.h>

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileSystemWatcher>
#include <QMetaEnum>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
