/* Precompiled header (BUILD.md) for the POSIX application signal bridge. */
#pragma once

#include "frontend/utility/platform.hpp"
#include "frontend/OBSApp.hpp"
#include <QPointer>
#include <QSocketNotifier>
#include <array>
#include <utility>
#include <cstdio>
#include <cstdlib>
#include <signal.h>
#include <pthread.h>
#include <sys/socket.h>
#include <unistd.h>
