// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_vulkan/present/window_adapt_pass.h"

namespace Vulkan {

std::unique_ptr<WindowAdaptPass> MakeNearestNeighbor(const Device& device,
                                                     const MemoryAllocator& memory_allocator,
                                                     size_t image_count, VkFormat frame_format);

std::unique_ptr<WindowAdaptPass> MakeBilinear(const Device& device,
                                              const MemoryAllocator& memory_allocator,
                                              size_t image_count, VkFormat frame_format);

std::unique_ptr<WindowAdaptPass> MakeBicubic(const Device& device,
                                             const MemoryAllocator& memory_allocator,
                                             size_t image_count, VkFormat frame_format);

std::unique_ptr<WindowAdaptPass> MakeGaussian(const Device& device,
                                              const MemoryAllocator& memory_allocator,
                                              size_t image_count, VkFormat frame_format);

std::unique_ptr<WindowAdaptPass> MakeScaleForce(const Device& device,
                                                const MemoryAllocator& memory_allocator,
                                                size_t image_count, VkFormat frame_format);

} // namespace Vulkan
