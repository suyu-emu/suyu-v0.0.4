// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/shader/shader_ir.h"

namespace Vulkan {

class VKDevice;

void FillDescriptorUpdateTemplateEntries(
    const VKDevice& device, const ShaderEntries& entries, u32& binding, u32& offset,
    std::vector<vk::DescriptorUpdateTemplateEntry>& template_entries);

} // namespace Vulkan
