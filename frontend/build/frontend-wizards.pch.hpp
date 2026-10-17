/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.wizards. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* wizards/AutoConfig.cpp */
#include "frontend/wizards/AutoConfig.hpp"
#include "frontend/wizards/AutoConfigStartPage.hpp"
#include "frontend/wizards/AutoConfigStreamPage.hpp"
#include "frontend/wizards/AutoConfigTestPage.hpp"
#include "frontend/wizards/AutoConfigVideoPage.hpp"
#include "ui_AutoConfigStartPage.h"
#include "ui_AutoConfigStreamPage.h"
#include "ui_AutoConfigVideoPage.h"

#ifdef YOUTUBE_ENABLED
#include <docks/YouTubeAppDock.hpp>
#include <utility/YoutubeApiWrappers.hpp>
#endif
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>

/* wizards/AutoConfigStreamPage.cpp */

#include <oauth/OAuth.hpp>
#include <utility/GoLiveAPI_Network.hpp>
#include <utility/GoLiveAPI_PostData.hpp>
#include <utility/MultitrackVideoError.hpp>
#ifdef YOUTUBE_ENABLED
#include <utility/YoutubeApiWrappers.hpp>
#endif

/* wizards/AutoConfigTestPage.cpp */
#include "frontend/wizards/TestMode.hpp"
#include "ui_AutoConfigTestPage.h"

#include <graphics/math-extra.h>

#include <QFormLayout>

/* wizards/AutoConfigVideoPage.cpp */

#include <OBSApp.hpp>

#include <QScreen>
