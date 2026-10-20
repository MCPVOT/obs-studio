/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.oauth. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* oauth/Auth.cpp */
#include "frontend/oauth/Auth.hpp"

#include <widgets/OBSBasic.hpp>

/* oauth/AuthListener.cpp */
#include "frontend/oauth/AuthListener.hpp"

#include <OBSApp.hpp>

#include <qt-wrappers.hpp>

#include <QRegularExpression>
#include <QTcpServer>
#include <QTcpSocket>

/* oauth/OAuth.cpp */
#include "frontend/oauth/OAuth.hpp"

#include <utility/RemoteTextThread.hpp>

#include <QMessageBox>
#include <json11.hpp>

#include "frontend/utility/platform.hpp"
