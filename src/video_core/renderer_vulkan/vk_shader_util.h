// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <string>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;

vk::ShaderModule BuildShader(const Device& device, std::span<const u32> code);

// Enhanced shader functionality
bool IsShaderValid(VkShaderModule shader_module);

void AsyncCompileShader(const Device& device, const std::string& shader_path,
                       std::function<void(VkShaderModule)> callback);

class ShaderManager {
public:
    explicit ShaderManager(const Device& device);
    ~ShaderManager();

    VkShaderModule GetShaderModule(const std::string& shader_path);
    void ReloadShader(const std::string& shader_path);
    bool LoadShader(const std::string& shader_path);
    void WaitForCompilation();

private:
    const Device& device;
    std::mutex shader_mutex;
    std::unordered_map<std::string, vk::ShaderModule> shader_cache;
};

} // namespace Vulkan
