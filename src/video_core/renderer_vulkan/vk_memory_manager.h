// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <utility>
#include <vector>
#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"

namespace Vulkan {

class VKDevice;
class VKMemoryAllocation;
class VKMemoryCommitImpl;

using VKMemoryCommit = std::unique_ptr<VKMemoryCommitImpl>;

class VKMemoryManager final {
public:
    explicit VKMemoryManager(const VKDevice& device);
    ~VKMemoryManager();

    /**
     * Commits a memory with the specified requeriments.
     * @param reqs Requeriments returned from a Vulkan call.
     * @param host_visible Signals the allocator that it *must* use host visible and coherent
     * memory. When passing false, it will try to allocate device local memory.
     * @returns A memory commit.
     */
    VKMemoryCommit Commit(const vk::MemoryRequirements& reqs, bool host_visible);

    /// Commits memory required by the buffer and binds it.
    VKMemoryCommit Commit(vk::Buffer buffer, bool host_visible);

    /// Commits memory required by the image and binds it.
    VKMemoryCommit Commit(vk::Image image, bool host_visible);

    /// Returns true if the memory allocations are done always in host visible and coherent memory.
    bool IsMemoryUnified() const {
        return is_memory_unified;
    }

private:
    /// Allocates a chunk of memory.
    bool AllocMemory(vk::MemoryPropertyFlags wanted_properties, u32 type_mask, u64 size);

    /// Returns true if the device uses an unified memory model.
    static bool GetMemoryUnified(const vk::PhysicalDeviceMemoryProperties& props);

    const VKDevice& device;                                  ///< Device handler.
    const vk::PhysicalDeviceMemoryProperties props;          ///< Physical device properties.
    const bool is_memory_unified;                            ///< True if memory model is unified.
    std::vector<std::unique_ptr<VKMemoryAllocation>> allocs; ///< Current allocations.
};

class VKMemoryCommitImpl final {
    friend VKMemoryAllocation;

public:
    explicit VKMemoryCommitImpl(VKMemoryAllocation* allocation, vk::DeviceMemory memory, u8* data,
                                u64 begin, u64 end);
    ~VKMemoryCommitImpl();

    /// Returns the writeable memory map. The commit has to be mappable.
    u8* GetData() const;

    /// Returns the Vulkan memory handler.
    vk::DeviceMemory GetMemory() const {
        return memory;
    }

    /// Returns the start position of the commit relative to the allocation.
    vk::DeviceSize GetOffset() const {
        return static_cast<vk::DeviceSize>(interval.first);
    }

private:
    std::pair<u64, u64> interval{};   ///< Interval where the commit exists.
    vk::DeviceMemory memory;          ///< Vulkan device memory handler.
    VKMemoryAllocation* allocation{}; ///< Pointer to the large memory allocation.
    u8* data{}; ///< Pointer to the host mapped memory, it has the commit offset included.
};

} // namespace Vulkan
