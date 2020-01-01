// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"

namespace Vulkan {

class VKDescriptorPool;

class DescriptorAllocator final : public VKFencedPool {
public:
    explicit DescriptorAllocator(VKDescriptorPool& descriptor_pool, vk::DescriptorSetLayout layout);
    ~DescriptorAllocator() override;

    DescriptorAllocator(const DescriptorAllocator&) = delete;

    vk::DescriptorSet Commit(VKFence& fence);

protected:
    void Allocate(std::size_t begin, std::size_t end) override;

private:
    VKDescriptorPool& descriptor_pool;
    const vk::DescriptorSetLayout layout;

    std::vector<UniqueDescriptorSet> descriptors;
};

class VKDescriptorPool final {
    friend DescriptorAllocator;

public:
    explicit VKDescriptorPool(const VKDevice& device);
    ~VKDescriptorPool();

private:
    vk::DescriptorPool AllocateNewPool();

    std::vector<UniqueDescriptorSet> AllocateDescriptors(vk::DescriptorSetLayout layout,
                                                         std::size_t count);

    const VKDevice& device;

    std::vector<UniqueDescriptorPool> pools;
    vk::DescriptorPool active_pool;
};

} // namespace Vulkan