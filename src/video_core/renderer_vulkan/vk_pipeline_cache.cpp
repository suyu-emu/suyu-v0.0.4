// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <vector>

#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"

namespace Vulkan {

void FillDescriptorUpdateTemplateEntries(
    const VKDevice& device, const ShaderEntries& entries, u32& binding, u32& offset,
    std::vector<vk::DescriptorUpdateTemplateEntry>& template_entries) {
    static constexpr auto entry_size = static_cast<u32>(sizeof(DescriptorUpdateEntry));
    const auto AddEntry = [&](vk::DescriptorType descriptor_type, std::size_t count_) {
        const u32 count = static_cast<u32>(count_);
        if (descriptor_type == vk::DescriptorType::eUniformTexelBuffer &&
            device.GetDriverID() == vk::DriverIdKHR::eNvidiaProprietary) {
            // Nvidia has a bug where updating multiple uniform texels at once causes the driver to
            // crash.
            for (u32 i = 0; i < count; ++i) {
                template_entries.emplace_back(binding + i, 0, 1, descriptor_type,
                                              offset + i * entry_size, entry_size);
            }
        } else if (count != 0) {
            template_entries.emplace_back(binding, 0, count, descriptor_type, offset, entry_size);
        }
        offset += count * entry_size;
        binding += count;
    };

    AddEntry(vk::DescriptorType::eUniformBuffer, entries.const_buffers.size());
    AddEntry(vk::DescriptorType::eStorageBuffer, entries.global_buffers.size());
    AddEntry(vk::DescriptorType::eUniformTexelBuffer, entries.texel_buffers.size());
    AddEntry(vk::DescriptorType::eCombinedImageSampler, entries.samplers.size());
    AddEntry(vk::DescriptorType::eStorageImage, entries.images.size());
}

} // namespace Vulkan
