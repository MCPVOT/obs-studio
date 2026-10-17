/* Precompiled header (BUILD.md) for the plugins/aja-output-ui unity groups
 * aja-output-ui.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* AJAOutputUI.cpp */
#include "plugins/aja-output-ui/AJAOutputUI.h"
#include "plugins/aja-output-ui/aja-ui-main.h"

#include "plugins/aja/aja-common.hpp"
#include "plugins/aja/aja-ui-props.hpp"
#include "plugins/aja/aja-enums.hpp"
#include "plugins/aja/aja-card-manager.hpp"

#include <ajantv2/includes/ntv2card.h>
#include <ajantv2/includes/ntv2devicefeatures.h>
#include <ajantv2/includes/ntv2enums.h>
#include <ajantv2/includes/ntv2utils.h>

#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>

/* aja-ui-main.cpp */

#include "plugins/aja/aja-routing.hpp"

#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include <ajantv2/includes/ntv2devicescanner.h>
