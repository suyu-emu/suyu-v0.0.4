// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Scheduler;

class AntiAliasPass {
public:
    virtual ~AntiAliasPass() = default;
    virtual VkImageView Draw(Scheduler& scheduler, size_t image_index, VkImage source_image,
                             VkImageView source_image_view) = 0;
};

class NoAA final : public AntiAliasPass {
public:
    virtual VkImageView Draw(Scheduler& scheduler, size_t image_index, VkImage source_image,
                             VkImageView source_image_view) {
        return source_image_view;
    }
};

} // namespace Vulkan
