// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <map>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;

class LsfgShaders {
public:
    explicit LsfgShaders(const Device& device);

    [[nodiscard]] bool IsValid() const {
        return valid;
    }

    [[nodiscard]] VkShaderModule Get(u32 shader_id) const;

private:
    std::map<u32, vk::ShaderModule> modules;
    bool valid{};
};

} // namespace Vulkan
