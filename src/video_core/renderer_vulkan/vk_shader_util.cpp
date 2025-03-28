// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-FileCopyrightText: Copyright 2025 citron Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>
#include <thread>
#include <filesystem>
#include <fstream>
#include <vector>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

vk::ShaderModule BuildShader(const Device& device, std::span<const u32> code) {
    return device.GetLogical().CreateShaderModule({
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = static_cast<u32>(code.size_bytes()),
        .pCode = code.data(),
    });
}

bool IsShaderValid(VkShaderModule shader_module) {
    // TODO: validate the shader by checking if it's null
    // or by examining SPIR-V data for correctness [ZEP]
    return shader_module != VK_NULL_HANDLE;
}

void AsyncCompileShader(const Device& device, const std::string& shader_path,
                       std::function<void(VkShaderModule)> callback) {
    LOG_INFO(Render_Vulkan, "Asynchronously compiling shader: {}", shader_path);

    // Since we can't copy Device directly, we'll load the shader synchronously instead
    // This is a simplified implementation that avoids threading complications
    try {
        // TODO: read SPIR-V from disk [ZEP]
        std::vector<u32> spir_v;
        bool success = false;

        // Check if the file exists and attempt to read it
        if (std::filesystem::exists(shader_path)) {
            std::ifstream shader_file(shader_path, std::ios::binary);
            if (shader_file) {
                shader_file.seekg(0, std::ios::end);
                size_t file_size = static_cast<size_t>(shader_file.tellg());
                shader_file.seekg(0, std::ios::beg);

                spir_v.resize(file_size / sizeof(u32));
                if (shader_file.read(reinterpret_cast<char*>(spir_v.data()), file_size)) {
                    success = true;
                }
            }
        }

        if (success) {
            vk::ShaderModule shader = BuildShader(device, spir_v);
            if (IsShaderValid(*shader)) {
                callback(*shader);
                return;
            }
        }

        LOG_ERROR(Render_Vulkan, "Shader compilation failed: {}", shader_path);
        callback(VK_NULL_HANDLE);
    } catch (const std::exception& e) {
        LOG_ERROR(Render_Vulkan, "Error compiling shader: {}", e.what());
        callback(VK_NULL_HANDLE);
    }
}

ShaderManager::ShaderManager(const Device& device) : device(device) {}

ShaderManager::~ShaderManager() {
    // Wait for any pending compilations to finish
    WaitForCompilation();

    // Clean up shader modules
    std::lock_guard<std::mutex> lock(shader_mutex);
    shader_cache.clear();
}

VkShaderModule ShaderManager::GetShaderModule(const std::string& shader_path) {
    std::lock_guard<std::mutex> lock(shader_mutex);
    auto it = shader_cache.find(shader_path);
    if (it != shader_cache.end()) {
        return *it->second;
    }

    // Try to load the shader if it's not in the cache
    if (LoadShader(shader_path)) {
        return *shader_cache[shader_path];
    }

    return VK_NULL_HANDLE;
}

void ShaderManager::ReloadShader(const std::string& shader_path) {
    LOG_INFO(Render_Vulkan, "Reloading shader: {}", shader_path);

    // Remove the old shader from cache
    {
        std::lock_guard<std::mutex> lock(shader_mutex);
        shader_cache.erase(shader_path);
    }

    // Load the shader again
    LoadShader(shader_path);
}

bool ShaderManager::LoadShader(const std::string& shader_path) {
    LOG_INFO(Render_Vulkan, "Loading shader from: {}", shader_path);

    try {
        // TODO: read SPIR-V from disk [ZEP]
        std::vector<u32> spir_v;
        bool success = false;

        // Check if the file exists and attempt to read it
        if (std::filesystem::exists(shader_path)) {
            std::ifstream shader_file(shader_path, std::ios::binary);
            if (shader_file) {
                shader_file.seekg(0, std::ios::end);
                size_t file_size = static_cast<size_t>(shader_file.tellg());
                shader_file.seekg(0, std::ios::beg);

                spir_v.resize(file_size / sizeof(u32));
                if (shader_file.read(reinterpret_cast<char*>(spir_v.data()), file_size)) {
                    success = true;
                }
            }
        }

        if (success) {
            vk::ShaderModule shader = BuildShader(device, spir_v);
            if (IsShaderValid(*shader)) {
                std::lock_guard<std::mutex> lock(shader_mutex);
                shader_cache[shader_path] = std::move(shader);
                return true;
            }
        }

        LOG_ERROR(Render_Vulkan, "Failed to load shader: {}", shader_path);
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR(Render_Vulkan, "Error loading shader: {}", e.what());
        return false;
    }
}

void ShaderManager::WaitForCompilation() {
    // No-op since compilation is now synchronous
    // The shader_compilation_in_progress flag isn't used anymore
}

} // namespace Vulkan
