// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"

namespace Vulkan {

// Prefer small grow rates to avoid saturating the descriptor pool with barely used pipelines.
static constexpr std::size_t SETS_GROW_RATE = 0x20;

DescriptorAllocator::DescriptorAllocator(VKDescriptorPool& descriptor_pool,
                                         vk::DescriptorSetLayout layout)
    : VKFencedPool{SETS_GROW_RATE}, descriptor_pool{descriptor_pool}, layout{layout} {}

DescriptorAllocator::~DescriptorAllocator() = default;

vk::DescriptorSet DescriptorAllocator::Commit(VKFence& fence) {
    return *descriptors[CommitResource(fence)];
}

void DescriptorAllocator::Allocate(std::size_t begin, std::size_t end) {
    auto new_sets = descriptor_pool.AllocateDescriptors(layout, end - begin);
    descriptors.insert(descriptors.end(), std::make_move_iterator(new_sets.begin()),
                       std::make_move_iterator(new_sets.end()));
}

VKDescriptorPool::VKDescriptorPool(const VKDevice& device)
    : device{device}, active_pool{AllocateNewPool()} {}

VKDescriptorPool::~VKDescriptorPool() = default;

vk::DescriptorPool VKDescriptorPool::AllocateNewPool() {
    static constexpr u32 num_sets = 0x20000;
    static constexpr vk::DescriptorPoolSize pool_sizes[] = {
        {vk::DescriptorType::eUniformBuffer, num_sets * 90},
        {vk::DescriptorType::eStorageBuffer, num_sets * 60},
        {vk::DescriptorType::eUniformTexelBuffer, num_sets * 64},
        {vk::DescriptorType::eCombinedImageSampler, num_sets * 64},
        {vk::DescriptorType::eStorageImage, num_sets * 40}};

    const vk::DescriptorPoolCreateInfo create_info(
        vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet, num_sets,
        static_cast<u32>(std::size(pool_sizes)), std::data(pool_sizes));
    const auto dev = device.GetLogical();
    return *pools.emplace_back(
        dev.createDescriptorPoolUnique(create_info, nullptr, device.GetDispatchLoader()));
}

std::vector<UniqueDescriptorSet> VKDescriptorPool::AllocateDescriptors(
    vk::DescriptorSetLayout layout, std::size_t count) {
    std::vector layout_copies(count, layout);
    vk::DescriptorSetAllocateInfo allocate_info(active_pool, static_cast<u32>(count),
                                                layout_copies.data());

    std::vector<vk::DescriptorSet> sets(count);
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    switch (const auto result = dev.allocateDescriptorSets(&allocate_info, sets.data(), dld)) {
    case vk::Result::eSuccess:
        break;
    case vk::Result::eErrorOutOfPoolMemory:
        active_pool = AllocateNewPool();
        allocate_info.descriptorPool = active_pool;
        if (dev.allocateDescriptorSets(&allocate_info, sets.data(), dld) == vk::Result::eSuccess) {
            break;
        }
        [[fallthrough]];
    default:
        vk::throwResultException(result, "vk::Device::allocateDescriptorSetsUnique");
    }

    vk::PoolFree deleter(dev, active_pool, dld);
    std::vector<UniqueDescriptorSet> unique_sets;
    unique_sets.reserve(count);
    for (const auto set : sets) {
        unique_sets.push_back(UniqueDescriptorSet{set, deleter});
    }
    return unique_sets;
}

} // namespace Vulkan
