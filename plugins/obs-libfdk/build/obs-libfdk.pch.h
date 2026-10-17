/* Precompiled header (BUILD.md) for the plugins/obs-libfdk unity groups
 * obs-libfdk.core. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* obs-libfdk.c */
#include <obs-module.h>

#ifdef DEBUG
#ifndef _DEBUG
#define _DEBUG
#endif
#undef DEBUG
#endif

#include <fdk-aac/aacenc_lib.h>
