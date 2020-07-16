// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

vk::ShaderModule BuildShader(const VKDevice& device, std::size_t code_size, const u8* code_data) {
    // Avoid undefined behavior by copying to a staging allocation
    ASSERT(code_size % sizeof(u32) == 0);
    const auto data = std::make_unique<u32[]>(code_size / sizeof(u32));
    std::memcpy(data.get(), code_data, code_size);

    return device.GetLogical().CreateShaderModule({
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = code_size,
        .pCode = data.get(),
    });
}

} // namespace Vulkan
