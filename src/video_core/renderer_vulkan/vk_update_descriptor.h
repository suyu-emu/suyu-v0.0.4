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

struct DescriptorUpdateEntry {
    DescriptorUpdateEntry(VkDescriptorImageInfo image_) : image{image_} {}

    DescriptorUpdateEntry(VkDescriptorBufferInfo buffer_) : buffer{buffer_} {}

    DescriptorUpdateEntry(VkBufferView texel_buffer_) : texel_buffer{texel_buffer_} {}

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
        payload.emplace_back(VkDescriptorImageInfo{sampler, image_view, {}});
    }

    void AddImage(VkImageView image_view) {
        payload.emplace_back(VkDescriptorImageInfo{{}, image_view, {}});
    }

    void AddBuffer(VkBuffer buffer, u64 offset, std::size_t size) {
        payload.emplace_back(VkDescriptorBufferInfo{buffer, offset, size});
    }

    void AddTexelBuffer(VkBufferView texel_buffer) {
        payload.emplace_back(texel_buffer);
    }

    VkImageLayout* LastImageLayout() {
        return &payload.back().image.imageLayout;
    }

    const VkImageLayout* LastImageLayout() const {
        return &payload.back().image.imageLayout;
    }

private:
    const VKDevice& device;
    VKScheduler& scheduler;

    const DescriptorUpdateEntry* upload_start = nullptr;
    boost::container::static_vector<DescriptorUpdateEntry, 0x10000> payload;
};

} // namespace Vulkan
