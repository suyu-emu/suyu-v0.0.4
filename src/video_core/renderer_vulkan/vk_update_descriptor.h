// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
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
    explicit VKUpdateDescriptorQueue(const Device& device_, VKScheduler& scheduler_);
    ~VKUpdateDescriptorQueue();

    void TickFrame();

    void Acquire();

    void Send(VkDescriptorUpdateTemplateKHR update_template, VkDescriptorSet set);

    void AddSampledImage(VkImageView image_view, VkSampler sampler) {
        payload.emplace_back(VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }

    void AddImage(VkImageView image_view) {
        payload.emplace_back(VkDescriptorImageInfo{
            .sampler = VK_NULL_HANDLE,
            .imageView = image_view,
            .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
        });
    }

    void AddBuffer(VkBuffer buffer, u64 offset, size_t size) {
        payload.emplace_back(VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size,
        });
    }

    void AddTexelBuffer(VkBufferView texel_buffer) {
        payload.emplace_back(texel_buffer);
    }

private:
    const Device& device;
    VKScheduler& scheduler;

    const DescriptorUpdateEntry* upload_start = nullptr;
    boost::container::static_vector<DescriptorUpdateEntry, 0x10000> payload;
};

} // namespace Vulkan
