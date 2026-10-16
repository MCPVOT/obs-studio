/******************************************************************************
Copyright (C) 2014 by Nibbles

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
******************************************************************************/

/* Platform defaults for the FreeType text source. One implementation per OS
 * (text-freetype2-platform-{windows,cocoa,unix}.c), picked by CMake. */
#pragma once

#include <stdint.h>

/* Extra obs_source_info output flags for this platform. */
uint32_t ft2_platform_source_flags(void);

/* The default font face. */
const char *ft2_platform_default_face(void);
