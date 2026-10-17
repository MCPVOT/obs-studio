/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.browserpanels. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* dialogs/OBSExtraBrowsers.cpp */
#include "frontend/dialogs/OBSExtraBrowsers.hpp"
#include "ui_OBSExtraBrowsers.h"

#include <utility/ExtraBrowsersDelegate.hpp>

/* docks/BrowserDock.cpp */
#include "frontend/docks/BrowserDock.hpp"

#include <QCloseEvent>

/* utility/ExtraBrowsersDelegate.cpp */
#include "frontend/utility/ExtraBrowsersDelegate.hpp"
#include "frontend/utility/ExtraBrowsersModel.hpp"

#include <components/EditWidget.hpp>

#include <qt-wrappers.hpp>

#include <QKeyEvent>

/* utility/ExtraBrowsersModel.cpp */

#include <components/DelButton.hpp>
#include <docks/BrowserDock.hpp>
#include <widgets/OBSBasic.hpp>

#include <QTableView>
