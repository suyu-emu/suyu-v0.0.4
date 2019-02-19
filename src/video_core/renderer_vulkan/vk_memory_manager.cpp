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

// TODO(Rodrigo): Fine tune this number
constexpr u64 ALLOC_CHUNK_SIZE = 64 * 1024 * 1024;

class VKMemoryAllocation final {
public:
    explicit VKMemoryAllocation(const VKDevice& device, vk::DeviceMemory memory,
                                vk::MemoryPropertyFlags properties, u64 alloc_size, u32 type)
        : device{device}, memory{memory}, properties{properties}, alloc_size{alloc_size},
          shifted_type{ShiftType(type)}, is_mappable{properties &
                                                     vk::MemoryPropertyFlagBits::eHostVisible} {
        if (is_mappable) {
            const auto dev = device.GetLogical();
            const auto& dld = device.GetDispatchLoader();
            base_address = static_cast<u8*>(dev.mapMemory(memory, 0, alloc_size, {}, dld));
        }
    }

    ~VKMemoryAllocation() {
        const auto dev = device.GetLogical();
        const auto& dld = device.GetDispatchLoader();
        if (is_mappable)
            dev.unmapMemory(memory, dld);
        dev.free(memory, nullptr, dld);
    }

    VKMemoryCommit Commit(vk::DeviceSize commit_size, vk::DeviceSize alignment) {
        auto found = TryFindFreeSection(free_iterator, alloc_size, static_cast<u64>(commit_size),
                                        static_cast<u64>(alignment));
        if (!found) {
            found = TryFindFreeSection(0, free_iterator, static_cast<u64>(commit_size),
                                       static_cast<u64>(alignment));
            if (!found) {
                // Signal out of memory, it'll try to do more allocations.
                return nullptr;
            }
        }
        u8* address = is_mappable ? base_address + *found : nullptr;
        auto commit = std::make_unique<VKMemoryCommitImpl>(this, memory, address, *found,
                                                           *found + commit_size);
        commits.push_back(commit.get());

        // Last commit's address is highly probable to be free.
        free_iterator = *found + commit_size;

        return commit;
    }

    void Free(const VKMemoryCommitImpl* commit) {
        ASSERT(commit);
        const auto it =
            std::find_if(commits.begin(), commits.end(),
                         [&](const auto& stored_commit) { return stored_commit == commit; });
        if (it == commits.end()) {
            LOG_CRITICAL(Render_Vulkan, "Freeing unallocated commit!");
            UNREACHABLE();
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
    /// requeriments.
    std::optional<u64> TryFindFreeSection(u64 start, u64 end, u64 size, u64 alignment) const {
        u64 iterator = start;
        while (iterator + size < end) {
            const u64 try_left = Common::AlignUp(iterator, alignment);
            const u64 try_right = try_left + size;

            bool overlap = false;
            for (const auto& commit : commits) {
                const auto [commit_left, commit_right] = commit->interval;
                if (try_left < commit_right && commit_left < try_right) {
                    // There's an overlap, continue the search where the overlapping commit ends.
                    iterator = commit_right;
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
    const u64 alloc_size;                     ///< Size of this allocation.
    const u32 shifted_type;                   ///< Stored Vulkan type of this allocation, shifted.
    const bool is_mappable;                   ///< Whether the allocation is mappable.

    /// Base address of the mapped pointer.
    u8* base_address{};

    /// Hints where the next free region is likely going to be.
    u64 free_iterator{};

    /// Stores all commits done from this allocation.
    std::vector<const VKMemoryCommitImpl*> commits;
};

VKMemoryManager::VKMemoryManager(const VKDevice& device)
    : device{device}, props{device.GetPhysical().getMemoryProperties(device.GetDispatchLoader())},
      is_memory_unified{GetMemoryUnified(props)} {}

VKMemoryManager::~VKMemoryManager() = default;

VKMemoryCommit VKMemoryManager::Commit(const vk::MemoryRequirements& reqs, bool host_visible) {
    ASSERT(reqs.size < ALLOC_CHUNK_SIZE);

    // When a host visible commit is asked, search for host visible and coherent, otherwise search
    // for a fast device local type.
    const vk::MemoryPropertyFlags wanted_properties =
        host_visible
            ? vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent
            : vk::MemoryPropertyFlagBits::eDeviceLocal;

    const auto TryCommit = [&]() -> VKMemoryCommit {
        for (auto& alloc : allocs) {
            if (!alloc->IsCompatible(wanted_properties, reqs.memoryTypeBits))
                continue;

            if (auto commit = alloc->Commit(reqs.size, reqs.alignment); commit) {
                return commit;
            }
        }
        return {};
    };

    if (auto commit = TryCommit(); commit) {
        return commit;
    }

    // Commit has failed, allocate more memory.
    if (!AllocMemory(wanted_properties, reqs.memoryTypeBits, ALLOC_CHUNK_SIZE)) {
        // TODO(Rodrigo): Try to use host memory.
        LOG_CRITICAL(Render_Vulkan, "Ran out of memory!");
        UNREACHABLE();
    }

    // Commit again, this time it won't fail since there's a fresh allocation above. If it does,
    // there's a bug.
    auto commit = TryCommit();
    ASSERT(commit);
    return commit;
}

VKMemoryCommit VKMemoryManager::Commit(vk::Buffer buffer, bool host_visible) {
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    const auto requeriments = dev.getBufferMemoryRequirements(buffer, dld);
    auto commit = Commit(requeriments, host_visible);
    dev.bindBufferMemory(buffer, commit->GetMemory(), commit->GetOffset(), dld);
    return commit;
}

VKMemoryCommit VKMemoryManager::Commit(vk::Image image, bool host_visible) {
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    const auto requeriments = dev.getImageMemoryRequirements(image, dld);
    auto commit = Commit(requeriments, host_visible);
    dev.bindImageMemory(image, commit->GetMemory(), commit->GetOffset(), dld);
    return commit;
}

bool VKMemoryManager::AllocMemory(vk::MemoryPropertyFlags wanted_properties, u32 type_mask,
                                  u64 size) {
    const u32 type = [&]() {
        for (u32 type_index = 0; type_index < props.memoryTypeCount; ++type_index) {
            const auto flags = props.memoryTypes[type_index].propertyFlags;
            if ((type_mask & (1U << type_index)) && (flags & wanted_properties)) {
                // The type matches in type and in the wanted properties.
                return type_index;
            }
        }
        LOG_CRITICAL(Render_Vulkan, "Couldn't find a compatible memory type!");
        UNREACHABLE();
        return 0u;
    }();

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();

    // Try to allocate found type.
    const vk::MemoryAllocateInfo memory_ai(size, type);
    vk::DeviceMemory memory;
    if (const vk::Result res = dev.allocateMemory(&memory_ai, nullptr, &memory, dld);
        res != vk::Result::eSuccess) {
        LOG_CRITICAL(Render_Vulkan, "Device allocation failed with code {}!", vk::to_string(res));
        return false;
    }
    allocs.push_back(
        std::make_unique<VKMemoryAllocation>(device, memory, wanted_properties, size, type));
    return true;
}

/*static*/ bool VKMemoryManager::GetMemoryUnified(const vk::PhysicalDeviceMemoryProperties& props) {
    for (u32 heap_index = 0; heap_index < props.memoryHeapCount; ++heap_index) {
        if (!(props.memoryHeaps[heap_index].flags & vk::MemoryHeapFlagBits::eDeviceLocal)) {
            // Memory is considered unified when heaps are device local only.
            return false;
        }
    }
    return true;
}

VKMemoryCommitImpl::VKMemoryCommitImpl(VKMemoryAllocation* allocation, vk::DeviceMemory memory,
                                       u8* data, u64 begin, u64 end)
    : allocation{allocation}, memory{memory}, data{data},
      interval(std::make_pair(begin, begin + end)) {}

VKMemoryCommitImpl::~VKMemoryCommitImpl() {
    allocation->Free(this);
}

u8* VKMemoryCommitImpl::GetData() const {
    ASSERT_MSG(data != nullptr, "Trying to access an unmapped commit.");
    return data;
}

} // namespace Vulkan
