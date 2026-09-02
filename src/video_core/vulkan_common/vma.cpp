// SPDX-FileCopyrightText: Copyright 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#define VMA_IMPLEMENTATION

// vk_mem_alloc.h is third-party (AMD's VulkanMemoryAllocator), and its own
// asserts intentionally compute values only used inside NDEBUG-gated
// assert() calls - real code, not dead code, but GCC's -Werror=unused-
// variable in a Release build (where assert() itself expands to nothing)
// flags it as if it were ours. Suppress it around this header only; suyu's
// own code stays subject to the full warning set everywhere else.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#pragma GCC diagnostic ignored "-Wconversion"
#endif

#include "video_core/vulkan_common/vma.h"

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
