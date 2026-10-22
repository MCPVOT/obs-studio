/* Precompiled header (BUILD.md) for the ported GIO groups libobs.ported-nix-dbus
 * and libobs.ported-nix-portal. Every implementation file in them includes this
 * header and nothing else: the includes and macro setup below are theirs, in
 * file order. */

#pragma once

/* util/platform-nix-dbus.c */
#include <assert.h>
#include <gio/gio.h>
#include "util/bmem.h"

/* util/platform-nix-portal.c */
#include "util/dstr.h"
