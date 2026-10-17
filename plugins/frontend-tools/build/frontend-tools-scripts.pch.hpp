/* Precompiled header (BUILD.md) for the plugins/frontend-tools unity groups
 * frontend-tools.scripts. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* scripts.cpp */
#include "obs-module.h"
#include "plugins/frontend-tools/scripts.hpp"
#include "plugins/frontend-tools/frontend-tools-host.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QPushButton>
#include <QFontDatabase>
#include <QFont>
#include <QDialogButtonBox>
#include <QResizeEvent>
#include <QAction>
#include <QMessageBox>
#include <QMenu>
#include <QUrl>
#include <QDesktopServices>
#include <qt-wrappers.hpp>
#include <plain-text-edit.hpp>
#include <properties-view.hpp>

#include <obs.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs-scripting.h>

#include <util/config-file.h>
#include <util/platform.h>
#include <util/util.hpp>

#include <string>

#include "ui_scripts.h"
