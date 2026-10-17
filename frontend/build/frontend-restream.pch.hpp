/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.restream. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* oauth/RestreamAuth.cpp */
#include "frontend/oauth/RestreamAuth.hpp"

#include <dialogs/OAuthLogin.hpp>
#include <docks/BrowserDock.hpp>
#include <utility/RemoteTextThread.hpp>
#include <utility/obf.h>
#include <widgets/OBSBasic.hpp>

#include <qt-wrappers.hpp>
#include <ui-config.h>

#include <json11.hpp>
