/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.dialogs, frontend.dialogs-2. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* dialogs/LogUploadDialog.cpp */
#include "frontend/dialogs/LogUploadDialog.hpp"

#include <OBSApp.hpp>

#include <QClipboard>
#include <QDesktopServices>
#include <QTimer>
#include <QUrlQuery>

/* dialogs/NameDialog.cpp */
#include "frontend/dialogs/NameDialog.hpp"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

/* dialogs/OAuthLogin.cpp */
#include "frontend/dialogs/OAuthLogin.hpp"

#include <widgets/OBSBasic.hpp>

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif
#include <ui-config.h>

/* dialogs/OBSAbout.cpp */
#include "frontend/dialogs/OBSAbout.hpp"

#include <utility/RemoteTextThread.hpp>
#include <utility/product-name.h>

#include <qt-wrappers.hpp>

#include <json11.hpp>

/* dialogs/OBSBasicAdvAudio.cpp */
#include "frontend/dialogs/OBSBasicAdvAudio.hpp"
#include "ui_OBSAdvAudio.h"

#include <components/OBSAdvAudioCtrl.hpp>
#include <utility/item-widget-helpers.hpp>

/* dialogs/OBSBasicSourceSelect.cpp */
#include "OBSApp.hpp"
#include "frontend/dialogs/OBSBasicSourceSelect.hpp"

#include <utility/ResizeSignaler.hpp>
#include <utility/source-select-list.hpp>
#include <utility/ThumbnailManager.hpp>
#include <utility/ThumbnailView.hpp>

#include "qt-wrappers.hpp"

#include <QList>
#include <QMessageBox>

/* dialogs/OBSBasicTransform.cpp */
#include "frontend/dialogs/OBSBasicTransform.hpp"

#include <DoubleSpinBox.hpp>

/* dialogs/OBSBasicVCamConfig.cpp */
#include "frontend/dialogs/OBSBasicVCamConfig.hpp"

/* dialogs/OBSLogViewer.cpp */
#include "frontend/dialogs/OBSLogViewer.hpp"

#include <QFile>
#include <QScrollBar>

/* dialogs/OBSMissingFiles.cpp */
#include "frontend/dialogs/OBSMissingFiles.hpp"

#include <utility/MissingFilesModel.hpp>
#include <utility/MissingFilesPathItemDelegate.hpp>

#include <QFileDialog>

/* dialogs/OBSRemux.cpp */
#include "frontend/dialogs/OBSRemux.hpp"

#include <utility/RemuxEntryPathItemDelegate.hpp>
#include <utility/RemuxQueueModel.hpp>
#include <utility/RemuxWorker.hpp>

#include <QDirIterator>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>

/* The classes' own headers, which their moc output used to include. */
#include "frontend/dialogs/OBSWhatsNew.hpp"

/* dialogs/OBSWhatsNew.cpp */
#include "frontend/dialogs/OBSWhatsNew.hpp"
#include <widgets/OBSBasic.hpp>
#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

#include "frontend/utility/platform.hpp"
