// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;

class DescriptorUpdateEntry {
public:
    explicit DescriptorUpdateEntry() {}

    DescriptorUpdateEntry(VkDescriptorImageInfo image) : image{image} {}

    DescriptorUpdateEntry(VkDescriptorBufferInfo buffer) : buffer{buffer} {}

    DescriptorUpdateEntry(VkBufferView texel_buffer) : texel_buffer{texel_buffer} {}

private:
    union {
        VkDescriptorImageInfo image;
        VkDescriptorBufferInfo buffer;
        VkBufferView texel_buffer;
    };
};

class VKUpdateDescriptorQueue final {
public:
    explicit VKUpdateDescriptorQueue(const VKDevice& device, VKScheduler& scheduler);
    ~VKUpdateDescriptorQueue();

    void TickFrame();

    void Acquire();

    void Send(VkDescriptorUpdateTemplateKHR update_template, VkDescriptorSet set);

    void AddSampledImage(VkSampler sampler, VkImageView image_view) {
        entries.emplace_back(VkDescriptorImageInfo{sampler, image_view, {}});
    }

    void AddImage(VkImageView image_view) {
        entries.emplace_back(VkDescriptorImageInfo{{}, image_view, {}});
    }

    void AddBuffer(VkBuffer buffer, u64 offset, std::size_t size) {
        entries.emplace_back(VkDescriptorBufferInfo{buffer, offset, size});
    }

    void AddTexelBuffer(VkBufferView texel_buffer) {
        entries.emplace_back(texel_buffer);
    }

    VkImageLayout* GetLastImageLayout() {
        return &std::get<VkDescriptorImageInfo>(entries.back()).imageLayout;
    }

private:
    using Variant = std::variant<VkDescriptorImageInfo, VkDescriptorBufferInfo, VkBufferView>;

    const VKDevice& device;
    VKScheduler& scheduler;

    boost::container::static_vector<Variant, 0x400> entries;
    boost::container::static_vector<DescriptorUpdateEntry, 0x10000> payload;
};

} // namespace Vulkan
