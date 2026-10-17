/* Unity group (BUILD.md): Windows host code. */

#include "libobs/build/libobs-windows.pch.h"

#include "libobs/audio-monitoring/win32/wasapi-enum-devices.c.h"
#include "libobs/audio-monitoring/win32/wasapi-monitoring-available.c.h"
#include "libobs/audio-monitoring/win32/wasapi-output.c.h"
#include "libobs/obs-core-modules-loader-win.c.h"
#include "libobs/obs-windows.c.h"
#include "libobs/util/windows/device-enum.c.h"
#include "libobs/util/windows/window-helpers.c.h"
