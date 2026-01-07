// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VK_NO_PROTOTYPES
#ifdef _WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif defined(__APPLE__)
#define VK_USE_PLATFORM_METAL_EXT
#elif defined(__ANDROID__)
#define VK_USE_PLATFORM_ANDROID_KHR
#elif defined(__HAIKU__)
#define VK_USE_PLATFORM_XCB_KHR
#else
#define VK_USE_PLATFORM_XLIB_KHR
#define VK_USE_PLATFORM_WAYLAND_KHR
#endif

#include <vulkan/vulkan.h>

// Define maintenance 7-9 extension names (not yet in official Vulkan headers)
#ifndef VK_KHR_MAINTENANCE_7_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_7_EXTENSION_NAME "VK_KHR_maintenance7"
#endif
#ifndef VK_KHR_MAINTENANCE_8_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_8_EXTENSION_NAME "VK_KHR_maintenance8"
#endif
#ifndef VK_KHR_MAINTENANCE_9_EXTENSION_NAME
#define VK_KHR_MAINTENANCE_9_EXTENSION_NAME "VK_KHR_maintenance9"
#endif

// Sanitize macros
#undef CreateEvent
#undef CreateSemaphore
#undef Always
#undef False
#undef None
#undef True
