/* Precompiled header (BUILD.md) for the plugins/decklink-output-ui unity groups
 * decklink-output-ui.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* DecklinkOutputUI.cpp */
#include "plugins/decklink-output-ui/DecklinkOutputUI.h"
#include <obs-module.h>
#include <util/platform.h>
#include <util/util.hpp>
#include "plugins/decklink-output-ui/decklink-ui-main.h"

/* decklink-ui-main.cpp */
#include <obs-frontend-api.h>
#include <QMainWindow>
#include <QAction>
#include <media-io/video-io.h>
#include <media-io/video-frame.h>
#include "plugins/decklink/const.h"
