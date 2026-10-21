/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.importers. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* importer/ImporterEntryPathItemDelegate.cpp */
#include "frontend/importer/ImporterEntryPathItemDelegate.hpp"
#include "frontend/importer/ImporterModel.hpp"

#include <OBSApp.hpp>

#include <qt-wrappers.hpp>

#include <QHBoxLayout>
#include <QLineEdit>
#include <QToolButton>

/* importer/ImporterModel.cpp */

#include <importers/importers.hpp>

/* importer/OBSImporter.cpp */
#include <utility/AppIdentity.hpp>
#include "frontend/importer/OBSImporter.hpp"

#include <models/SceneCollection.hpp>
#include <widgets/OBSBasic.hpp>

#include <QDirIterator>
#include <QDropEvent>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>

/* importers/classic.cpp */
#include "frontend/importers/importers.hpp"

#include <QByteArray>

/* importers/importers.cpp */
#include <memory>

/* importers/studio.cpp */

/* importers/xsplit.cpp */
#include <ctype.h>

#include <QDomDocument>
