/* Precompiled header (BUILD.md) for the plugins/win-capture/graphics-hook unity groups
 * graphics-hook.vulkan-capture-windows. C. Every implementation file in them includes this header and
 * nothing else: the includes and macro setup below are theirs, in group and
 * file order. */

#pragma once

/* vulkan-capture.c */
#include <windows.h>
#include "plugins/win-capture/graphics-hook/graphics-hook.h"

#define VK_USE_PLATFORM_WIN32_KHR

#include <malloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_layer.h>

#include <vulkan/vulkan_win32.h>

#define COBJMACROS
#include <dxgi.h>
#include <d3d11.h>

#include "plugins/win-capture/graphics-hook/dxgi-helpers.hpp"

#include "plugins/win-capture/graphics-hook/vulkan-capture.h"
