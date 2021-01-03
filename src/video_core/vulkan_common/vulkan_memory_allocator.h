// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <span>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class MemoryMap;
class MemoryAllocation;

class MemoryCommit final {
public:
    explicit MemoryCommit() noexcept = default;
    explicit MemoryCommit(const Device& device_, MemoryAllocation* allocation_,
                          VkDeviceMemory memory_, u64 begin, u64 end) noexcept;
    ~MemoryCommit();

    MemoryCommit& operator=(MemoryCommit&&) noexcept;
    MemoryCommit(MemoryCommit&&) noexcept;

    MemoryCommit& operator=(const MemoryCommit&) = delete;
    MemoryCommit(const MemoryCommit&) = delete;

    /// Returns a host visible memory map.
    /// It will map the backing allocation if it hasn't been mapped before.
    std::span<u8> Map();

    /// Returns the Vulkan memory handler.
    VkDeviceMemory Memory() const {
        return memory;
    }

    /// Returns the start position of the commit relative to the allocation.
    VkDeviceSize Offset() const {
        return static_cast<VkDeviceSize>(interval.first);
    }

private:
    void Release();

    const Device* device{};         ///< Vulkan device.
    MemoryAllocation* allocation{}; ///< Pointer to the large memory allocation.
    VkDeviceMemory memory{};        ///< Vulkan device memory handler.
    std::pair<u64, u64> interval{}; ///< Interval where the commit exists.
    std::span<u8> span;             ///< Host visible memory span. Empty if not queried before.
};

class MemoryAllocator final {
public:
    explicit MemoryAllocator(const Device& device_);
    ~MemoryAllocator();

    MemoryAllocator& operator=(const MemoryAllocator&) = delete;
    MemoryAllocator(const MemoryAllocator&) = delete;

    /**
     * Commits a memory with the specified requeriments.
     *
     * @param requirements Requirements returned from a Vulkan call.
     * @param host_visible Signals the allocator that it *must* use host visible and coherent
     *                     memory. When passing false, it will try to allocate device local memory.
     *
     * @returns A memory commit.
     */
    MemoryCommit Commit(const VkMemoryRequirements& requirements, bool host_visible);

    /// Commits memory required by the buffer and binds it.
    MemoryCommit Commit(const vk::Buffer& buffer, bool host_visible);

    /// Commits memory required by the image and binds it.
    MemoryCommit Commit(const vk::Image& image, bool host_visible);

private:
    /// Allocates a chunk of memory.
    void AllocMemory(VkMemoryPropertyFlags wanted_properties, u32 type_mask, u64 size);

    /// Tries to allocate a memory commit.
    std::optional<MemoryCommit> TryAllocCommit(const VkMemoryRequirements& requirements,
                                               VkMemoryPropertyFlags wanted_properties);

    const Device& device;                                       ///< Device handler.
    const VkPhysicalDeviceMemoryProperties properties;          ///< Physical device properties.
    std::vector<std::unique_ptr<MemoryAllocation>> allocations; ///< Current allocations.
};

} // namespace Vulkan
