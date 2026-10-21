/* Precompiled header (BUILD.md) for the shared/properties-view unity groups
 * properties-view.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* double-slider.cpp */
#include <cmath>

/* properties-view.cpp */
#include <QFormLayout>
#include <QScrollBar>
#include <QLabel>
#include <QCheckBox>
#include <QFont>
#include <QFontDialog>
#include <QLineEdit>
#include <QSpinBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QStandardItem>
#include <QFileDialog>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QMenu>
#include <QMessageBox>
#include <QStackedWidget>
#include <QDir>
#include <QGroupBox>
#include <QObject>
#include <QDesktopServices>
#include <QUuid>

#include "DoubleSpinBox.hpp"
#include "SpinBox.hpp"
#include "shared/properties-view/double-slider.hpp"
#include "shared/properties-view/spinbox-ignorewheel.hpp"
#include "shared/properties-view/properties-view.moc.hpp"

#include <qt-wrappers.hpp>
#include <plain-text-edit.hpp>
#include <slider-ignorewheel.hpp>
#include <IconLabel.hpp>
#include <cstdlib>
#include <initializer_list>
#include <obs-data.h>
#include <obs.h>
#include <qtimer.h>
#include <string>
#include <obs-frontend-api.h>

/* spinbox-ignorewheel.cpp (its moc output used to include this) */
#include "shared/properties-view/spinbox-ignorewheel.hpp"

/* The classes' own headers, which their moc output used to include. */
#include "shared/properties-view/properties-view.hpp"

#include "shared/qt/platform/qt-platform.hpp"
