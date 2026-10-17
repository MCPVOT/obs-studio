/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.whatsnew. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/WhatsNewBrowserInitThread.cpp */
#include "frontend/utility/WhatsNewBrowserInitThread.hpp"

#ifdef BROWSER_AVAILABLE
#include <browser-panel.hpp>
#endif

/* utility/WhatsNewInfoThread.cpp */
#include "frontend/utility/WhatsNewInfoThread.hpp"

#include <OBSApp.hpp>
#include <utility/RemoteTextThread.hpp>
#include <utility/crypto-helpers.hpp>
#include <utility/platform.hpp>
#include <utility/update-helpers.hpp>

#include <QRandomGenerator>
#include <blake2.h>

#include <fstream>
#include <mutex>

/* utility/update-helpers.cpp */
#include "frontend/utility/update-helpers.hpp"

#include <stdarg.h>

/* The classes' own headers, which their moc output used to include. */
#include "frontend/utility/WhatsNewBrowserInitThread.hpp"
#include "frontend/utility/WhatsNewInfoThread.hpp"
