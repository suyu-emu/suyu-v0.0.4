// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDescriptorPool;

class DescriptorAllocator final : public VKFencedPool {
public:
    explicit DescriptorAllocator(VKDescriptorPool& descriptor_pool, VkDescriptorSetLayout layout);
    ~DescriptorAllocator() override;

    DescriptorAllocator(const DescriptorAllocator&) = delete;

    VkDescriptorSet Commit(VKFence& fence);

protected:
    void Allocate(std::size_t begin, std::size_t end) override;

private:
    VKDescriptorPool& descriptor_pool;
    const VkDescriptorSetLayout layout;

    std::vector<vk::DescriptorSets> descriptors_allocations;
};

class VKDescriptorPool final {
    friend DescriptorAllocator;

public:
    explicit VKDescriptorPool(const VKDevice& device);
    ~VKDescriptorPool();

private:
    vk::DescriptorPool* AllocateNewPool();

    vk::DescriptorSets AllocateDescriptors(VkDescriptorSetLayout layout, std::size_t count);

    const VKDevice& device;

    std::vector<vk::DescriptorPool> pools;
    vk::DescriptorPool* active_pool;
};

} // namespace Vulkan