/* Precompiled header (BUILD.md) for the plugins/frontend-tools unity groups
 * frontend-tools.core. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* auto-scene-switcher.cpp */
#include <obs-frontend-api.h>
#include <obs-module.h>
#include <obs.hpp>
#include <util/util.hpp>
#include <QMainWindow>
#include <QMessageBox>
#include <QAction>
#include "plugins/frontend-tools/auto-scene-switcher.hpp"
#include "plugins/frontend-tools/tool-helpers.hpp"

#include <condition_variable>
#include <chrono>
#include <string>
#include <vector>
#include <thread>
#include <regex>
#include <mutex>
