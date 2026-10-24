/* Precompiled header (BUILD.md) for the frontend unity groups
 * frontend.x11. C++. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* utility/platform-x11.cpp */
#include "frontend/utility/platform.hpp"

#include <OBSApp.hpp>

#include <obs-config.h>
#include <obs-nix-platform.h>
#include <util/bmem.h>
#include <util/platform.h>

#include <QApplication>
#include <QDBusConnection>
#include <QDBusReply>
#include <QIcon>
#include <QScreen>

#include <dlfcn.h>

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <condition_variable>
#include <mutex>
#endif
#include <sstream>
#include <string>
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <thread>
#endif
#include <vector>

#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <fcntl.h>
#include <sys/param.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <libprocstat.h>
#include <pthread_np.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/param.h>
#endif
#ifdef __linux__
#include <sys/socket.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/sysctl.h>
#endif
#ifdef __linux__
#include <sys/un.h>
#endif
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <sys/user.h>
#endif
#include <unistd.h>

/* utility/system-info-posix.cpp */
#include "frontend/utility/system-info.hpp"
#include "util/dstr.hpp"
#include "util/platform.h"
#include <sys/utsname.h>
#include <fstream>
#include <regex>

/* utility/system-info-posix.cpp: libpci is C */
extern "C" {
#include "pci/pci.h"
}

#include "json11.hpp"

#include "frontend/utility/OBSProxyStyle.hpp"

#include <QWindow>
