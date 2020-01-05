// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>
#include <variant>
#include <boost/container/static_vector.hpp>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;

class DescriptorUpdateEntry {
public:
    explicit DescriptorUpdateEntry() : image{} {}

    DescriptorUpdateEntry(vk::DescriptorImageInfo image) : image{image} {}

    DescriptorUpdateEntry(vk::Buffer buffer, vk::DeviceSize offset, vk::DeviceSize size)
        : buffer{buffer, offset, size} {}

    DescriptorUpdateEntry(vk::BufferView texel_buffer) : texel_buffer{texel_buffer} {}

private:
    union {
        vk::DescriptorImageInfo image;
        vk::DescriptorBufferInfo buffer;
        vk::BufferView texel_buffer;
    };
};

class VKUpdateDescriptorQueue final {
public:
    explicit VKUpdateDescriptorQueue(const VKDevice& device, VKScheduler& scheduler);
    ~VKUpdateDescriptorQueue();

    void TickFrame();

    void Acquire();

    void Send(vk::DescriptorUpdateTemplate update_template, vk::DescriptorSet set);

    void AddSampledImage(vk::Sampler sampler, vk::ImageView image_view) {
        entries.emplace_back(vk::DescriptorImageInfo{sampler, image_view, {}});
    }

    void AddImage(vk::ImageView image_view) {
        entries.emplace_back(vk::DescriptorImageInfo{{}, image_view, {}});
    }

    void AddBuffer(const vk::Buffer* buffer, u64 offset, std::size_t size) {
        entries.push_back(Buffer{buffer, offset, size});
    }

    void AddTexelBuffer(vk::BufferView texel_buffer) {
        entries.emplace_back(texel_buffer);
    }

    vk::ImageLayout* GetLastImageLayout() {
        return &std::get<vk::DescriptorImageInfo>(entries.back()).imageLayout;
    }

private:
    struct Buffer {
        const vk::Buffer* buffer{};
        u64 offset{};
        std::size_t size{};
    };
    using Variant = std::variant<vk::DescriptorImageInfo, Buffer, vk::BufferView>;
    // Old gcc versions don't consider this trivially copyable.
    // static_assert(std::is_trivially_copyable_v<Variant>);

    const VKDevice& device;
    VKScheduler& scheduler;

    boost::container::static_vector<Variant, 0x400> entries;
    boost::container::static_vector<DescriptorUpdateEntry, 0x10000> payload;
};

} // namespace Vulkan
