/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.twitch. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* oauth/TwitchAuth.cpp */
#include "frontend/oauth/TwitchAuth.hpp"

#include <dialogs/OAuthLogin.hpp>
#include <docks/BrowserDock.hpp>
#include <utility/RemoteTextThread.hpp>
#include <utility/obf.h>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>
#include <ui-config.h>

#include <QTimer>
#include <QUuid>
