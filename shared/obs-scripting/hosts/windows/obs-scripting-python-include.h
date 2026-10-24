#pragma once

/* Python.h on Windows (obs-rust/obs-studio#882), with _DEBUG hidden: a Debug
 * build of OBS must not make Python ask for its debug library. */
#ifdef _DEBUG
#undef _DEBUG
#include <Python.h>
#define _DEBUG
#else
#include <Python.h>
#endif
