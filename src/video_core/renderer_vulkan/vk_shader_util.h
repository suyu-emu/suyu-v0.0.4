// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;

vk::ShaderModule BuildShader(const VKDevice& device, std::size_t code_size, const u8* code_data);

} // namespace Vulkan
