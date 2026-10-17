/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.components-sourcetreeitem. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* components/SourceTreeItem.cpp */
#include "frontend/components/SourceTreeItem.hpp"

#include <components/OBSSourceLabel.hpp>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>
#include <Idian/Utils.hpp>

#include <QCheckBox>
#include <QLineEdit>
#include <QPainter>

#include "plugin-manager/PluginManager.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
