// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <optional>
#include <tuple>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"

namespace Vulkan {

namespace {

u64 GetAllocationChunkSize(u64 required_size) {
    static constexpr u64 sizes[] = {16ULL << 20, 32ULL << 20, 64ULL << 20, 128ULL << 20};
    auto it = std::lower_bound(std::begin(sizes), std::end(sizes), required_size);
    return it != std::end(sizes) ? *it : Common::AlignUp(required_size, 256ULL << 20);
}

} // Anonymous namespace

class VKMemoryAllocation final {
public:
    explicit VKMemoryAllocation(const VKDevice& device, vk::DeviceMemory memory,
                                vk::MemoryPropertyFlags properties, u64 allocation_size, u32 type)
        : device{device}, memory{memory}, properties{properties}, allocation_size{allocation_size},
          shifted_type{ShiftType(type)} {}

    ~VKMemoryAllocation() {
        const auto dev = device.GetLogical();
        const auto& dld = device.GetDispatchLoader();
        dev.free(memory, nullptr, dld);
    }

    VKMemoryCommit Commit(vk::DeviceSize commit_size, vk::DeviceSize alignment) {
        auto found = TryFindFreeSection(free_iterator, allocation_size,
                                        static_cast<u64>(commit_size), static_cast<u64>(alignment));
        if (!found) {
            found = TryFindFreeSection(0, free_iterator, static_cast<u64>(commit_size),
                                       static_cast<u64>(alignment));
            if (!found) {
                // Signal out of memory, it'll try to do more allocations.
                return nullptr;
            }
        }
        auto commit = std::make_unique<VKMemoryCommitImpl>(device, this, memory, *found,
                                                           *found + commit_size);
        commits.push_back(commit.get());

        // Last commit's address is highly probable to be free.
        free_iterator = *found + commit_size;

        return commit;
    }

    void Free(const VKMemoryCommitImpl* commit) {
        ASSERT(commit);

        const auto it = std::find(std::begin(commits), std::end(commits), commit);
        if (it == commits.end()) {
            UNREACHABLE_MSG("Freeing unallocated commit!");
            return;
        }
        commits.erase(it);
    }

    /// Returns whether this allocation is compatible with the arguments.
    bool IsCompatible(vk::MemoryPropertyFlags wanted_properties, u32 type_mask) const {
        return (wanted_properties & properties) != vk::MemoryPropertyFlagBits(0) &&
               (type_mask & shifted_type) != 0;
    }

private:
    static constexpr u32 ShiftType(u32 type) {
        return 1U << type;
    }

    /// A memory allocator, it may return a free region between "start" and "end" with the solicited
    /// requirements.
    std::optional<u64> TryFindFreeSection(u64 start, u64 end, u64 size, u64 alignment) const {
        u64 iterator = Common::AlignUp(start, alignment);
        while (iterator + size <= end) {
            const u64 try_left = iterator;
            const u64 try_right = try_left + size;

            bool overlap = false;
            for (const auto& commit : commits) {
                const auto [commit_left, commit_right] = commit->interval;
                if (try_left < commit_right && commit_left < try_right) {
                    // There's an overlap, continue the search where the overlapping commit ends.
                    iterator = Common::AlignUp(commit_right, alignment);
                    overlap = true;
                    break;
                }
            }
            if (!overlap) {
                // A free address has been found.
                return try_left;
            }
        }

        // No free regions where found, return an empty optional.
        return std::nullopt;
    }

    const VKDevice& device;                   ///< Vulkan device.
    const vk::DeviceMemory memory;            ///< Vulkan memory allocation handler.
    const vk::MemoryPropertyFlags properties; ///< Vulkan properties.
    const u64 allocation_size;                ///< Size of this allocation.
    const u32 shifted_type;                   ///< Stored Vulkan type of this allocation, shifted.

    /// Hints where the next free region is likely going to be.
    u64 free_iterator{};

    /// Stores all commits done from this allocation.
    std::vector<const VKMemoryCommitImpl*> commits;
};

VKMemoryManager::VKMemoryManager(const VKDevice& device)
    : device{device}, properties{device.GetPhysical().getMemoryProperties(
                          device.GetDispatchLoader())},
      is_memory_unified{GetMemoryUnified(properties)} {}

VKMemoryManager::~VKMemoryManager() = default;

VKMemoryCommit VKMemoryManager::Commit(const vk::MemoryRequirements& requirements,
                                       bool host_visible) {
    const u64 chunk_size = GetAllocationChunkSize(requirements.size);

    // When a host visible commit is asked, search for host visible and coherent, otherwise search
    // for a fast device local type.
    const vk::MemoryPropertyFlags wanted_properties =
        host_visible
            ? vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            : vk::MemoryPropertyFlagBits::eDeviceLocal;

    if (auto commit = TryAllocCommit(requirements, wanted_properties)) {
        return commit;
    }

    // Commit has failed, allocate more memory.
    if (!AllocMemory(wanted_properties, requirements.memoryTypeBits, chunk_size)) {
        // TODO(Rodrigo): Handle these situations in some way like flushing to guest memory.
        // Allocation has failed, panic.
        UNREACHABLE_MSG("Ran out of VRAM!");
        return {};
    }

    // Commit again, this time it won't fail since there's a fresh allocation above. If it does,
    // there's a bug.
    auto commit = TryAllocCommit(requirements, wanted_properties);
    ASSERT(commit);
    return commit;
}

VKMemoryCommit VKMemoryManager::Commit(vk::Buffer buffer, bool host_visible) {
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    auto commit = Commit(dev.getBufferMemoryRequirements(buffer, dld), host_visible);
    dev.bindBufferMemory(buffer, commit->GetMemory(), commit->GetOffset(), dld);
    return commit;
}

VKMemoryCommit VKMemoryManager::Commit(vk::Image image, bool host_visible) {
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    auto commit = Commit(dev.getImageMemoryRequirements(image, dld), host_visible);
    dev.bindImageMemory(image, commit->GetMemory(), commit->GetOffset(), dld);
    return commit;
}

bool VKMemoryManager::AllocMemory(vk::MemoryPropertyFlags wanted_properties, u32 type_mask,
                                  u64 size) {
    const u32 type = [&] {
        for (u32 type_index = 0; type_index < properties.memoryTypeCount; ++type_index) {
            const auto flags = properties.memoryTypes[type_index].propertyFlags;
            if ((type_mask & (1U << type_index)) && (flags & wanted_properties)) {
                // The type matches in type and in the wanted properties.
                return type_index;
            }
        }
        UNREACHABLE_MSG("Couldn't find a compatible memory type!");
        return 0U;
    }();

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();

    // Try to allocate found type.
    const vk::MemoryAllocateInfo memory_ai(size, type);
    vk::DeviceMemory memory;
    if (const auto res = dev.allocateMemory(&memory_ai, nullptr, &memory, dld);
        res != vk::Result::eSuccess) {
        LOG_CRITICAL(Render_Vulkan, "Device allocation failed with code {}!", vk::to_string(res));
        return false;
    }
    allocations.push_back(
        std::make_unique<VKMemoryAllocation>(device, memory, wanted_properties, size, type));
    return true;
}

VKMemoryCommit VKMemoryManager::TryAllocCommit(const vk::MemoryRequirements& requirements,
                                               vk::MemoryPropertyFlags wanted_properties) {
    for (auto& allocation : allocations) {
        if (!allocation->IsCompatible(wanted_properties, requirements.memoryTypeBits)) {
            continue;
        }
        if (auto commit = allocation->Commit(requirements.size, requirements.alignment)) {
            return commit;
        }
    }
    return {};
}

/*static*/ bool VKMemoryManager::GetMemoryUnified(
    const vk::PhysicalDeviceMemoryProperties& properties) {
    for (u32 heap_index = 0; heap_index < properties.memoryHeapCount; ++heap_index) {
        if (!(properties.memoryHeaps[heap_index].flags & vk::MemoryHeapFlagBits::eDeviceLocal)) {
            // Memory is considered unified when heaps are device local only.
            return false;
        }
    }
    return true;
}

VKMemoryCommitImpl::VKMemoryCommitImpl(const VKDevice& device, VKMemoryAllocation* allocation,
                                       vk::DeviceMemory memory, u64 begin, u64 end)
    : device{device}, interval{begin, end}, memory{memory}, allocation{allocation} {}

VKMemoryCommitImpl::~VKMemoryCommitImpl() {
    allocation->Free(this);
}

MemoryMap VKMemoryCommitImpl::Map(u64 size, u64 offset_) const {
    const auto dev = device.GetLogical();
    const auto address = reinterpret_cast<u8*>(
        dev.mapMemory(memory, interval.first + offset_, size, {}, device.GetDispatchLoader()));
    return MemoryMap{this, address};
}

void VKMemoryCommitImpl::Unmap() const {
    const auto dev = device.GetLogical();
    dev.unmapMemory(memory, device.GetDispatchLoader());
}

MemoryMap VKMemoryCommitImpl::Map() const {
    return Map(interval.second - interval.first);
}

} // namespace Vulkan
