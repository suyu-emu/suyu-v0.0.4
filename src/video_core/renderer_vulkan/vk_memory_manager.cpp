// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <bit>
#include <optional>
#include <tuple>
#include <vector>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {
namespace {
struct Range {
    u64 begin;
    u64 end;

    [[nodiscard]] bool Contains(u64 iterator, u64 size) const noexcept {
        return iterator < end && begin < iterator + size;
    }
};

[[nodiscard]] u64 GetAllocationChunkSize(u64 required_size) {
    static constexpr std::array sizes{
        0x1000ULL << 10,  0x1400ULL << 10,  0x1800ULL << 10,  0x1c00ULL << 10, 0x2000ULL << 10,
        0x3200ULL << 10,  0x4000ULL << 10,  0x6000ULL << 10,  0x8000ULL << 10, 0xA000ULL << 10,
        0x10000ULL << 10, 0x18000ULL << 10, 0x20000ULL << 10,
    };
    static_assert(std::is_sorted(sizes.begin(), sizes.end()));

    const auto it = std::ranges::lower_bound(sizes, required_size);
    return it != sizes.end() ? *it : Common::AlignUp(required_size, 4ULL << 20);
}
} // Anonymous namespace

class MemoryAllocation {
public:
    explicit MemoryAllocation(const Device& device_, vk::DeviceMemory memory_,
                              VkMemoryPropertyFlags properties_, u64 allocation_size_, u32 type_)
        : device{device_}, memory{std::move(memory_)}, properties{properties_},
          allocation_size{allocation_size_}, shifted_type{ShiftType(type_)} {}

    [[nodiscard]] std::optional<MemoryCommit> Commit(VkDeviceSize size, VkDeviceSize alignment) {
        const std::optional<u64> alloc = FindFreeRegion(size, alignment);
        if (!alloc) {
            // Signal out of memory, it'll try to do more allocations.
            return std::nullopt;
        }
        const Range range{
            .begin = *alloc,
            .end = *alloc + size,
        };
        commits.insert(std::ranges::upper_bound(commits, *alloc, {}, &Range::begin), range);
        return std::make_optional<MemoryCommit>(device, this, *memory, *alloc, *alloc + size);
    }

    void Free(u64 begin) {
        const auto it = std::ranges::find(commits, begin, &Range::begin);
        ASSERT_MSG(it != commits.end(), "Invalid commit");
        commits.erase(it);
    }

    [[nodiscard]] std::span<u8> Map() {
        if (!memory_mapped_span.empty()) {
            return memory_mapped_span;
        }
        u8* const raw_pointer = memory.Map(0, allocation_size);
        memory_mapped_span = std::span<u8>(raw_pointer, allocation_size);
        return memory_mapped_span;
    }

    /// Returns whether this allocation is compatible with the arguments.
    [[nodiscard]] bool IsCompatible(VkMemoryPropertyFlags wanted_properties, u32 type_mask) const {
        return (wanted_properties & properties) && (type_mask & shifted_type) != 0;
    }

private:
    [[nodiscard]] static constexpr u32 ShiftType(u32 type) {
        return 1U << type;
    }

    [[nodiscard]] std::optional<u64> FindFreeRegion(u64 size, u64 alignment) noexcept {
        ASSERT(std::has_single_bit(alignment));
        const u64 alignment_log2 = std::countr_zero(alignment);
        std::optional<u64> candidate;
        u64 iterator = 0;
        auto commit = commits.begin();
        while (iterator + size <= allocation_size) {
            candidate = candidate.value_or(iterator);
            if (commit == commits.end()) {
                break;
            }
            if (commit->Contains(*candidate, size)) {
                candidate = std::nullopt;
            }
            iterator = Common::AlignUpLog2(commit->end, alignment_log2);
            ++commit;
        }
        return candidate;
    }

    const Device& device;                   ///< Vulkan device.
    const vk::DeviceMemory memory;          ///< Vulkan memory allocation handler.
    const VkMemoryPropertyFlags properties; ///< Vulkan properties.
    const u64 allocation_size;              ///< Size of this allocation.
    const u32 shifted_type;                 ///< Stored Vulkan type of this allocation, shifted.
    std::vector<Range> commits;             ///< All commit ranges done from this allocation.
    std::span<u8> memory_mapped_span;       ///< Memory mapped span. Empty if not queried before.
};

MemoryCommit::MemoryCommit(const Device& device_, MemoryAllocation* allocation_,
                           VkDeviceMemory memory_, u64 begin, u64 end) noexcept
    : device{&device_}, allocation{allocation_}, memory{memory_}, interval{begin, end} {}

MemoryCommit::~MemoryCommit() {
    Release();
}

MemoryCommit& MemoryCommit::operator=(MemoryCommit&& rhs) noexcept {
    Release();
    device = rhs.device;
    allocation = std::exchange(rhs.allocation, nullptr);
    memory = rhs.memory;
    interval = rhs.interval;
    span = std::exchange(rhs.span, std::span<u8>{});
    return *this;
}

MemoryCommit::MemoryCommit(MemoryCommit&& rhs) noexcept
    : device{rhs.device}, allocation{std::exchange(rhs.allocation, nullptr)}, memory{rhs.memory},
      interval{rhs.interval}, span{std::exchange(rhs.span, std::span<u8>{})} {}

std::span<u8> MemoryCommit::Map() {
    if (!span.empty()) {
        return span;
    }
    span = allocation->Map().subspan(interval.first, interval.second - interval.first);
    return span;
}

void MemoryCommit::Release() {
    if (allocation) {
        allocation->Free(interval.first);
    }
}

MemoryAllocator::MemoryAllocator(const Device& device_)
    : device{device_}, properties{device_.GetPhysical().GetMemoryProperties()} {}

MemoryAllocator::~MemoryAllocator() = default;

MemoryCommit MemoryAllocator::Commit(const VkMemoryRequirements& requirements, bool host_visible) {
    const u64 chunk_size = GetAllocationChunkSize(requirements.size);

    // When a host visible commit is asked, search for host visible and coherent, otherwise search
    // for a fast device local type.
    const VkMemoryPropertyFlags wanted_properties =
        host_visible ? VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
                     : VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (std::optional<MemoryCommit> commit = TryAllocCommit(requirements, wanted_properties)) {
        return std::move(*commit);
    }
    // Commit has failed, allocate more memory.
    // TODO(Rodrigo): Handle out of memory situations in some way like flushing to guest memory.
    AllocMemory(wanted_properties, requirements.memoryTypeBits, chunk_size);

    // Commit again, this time it won't fail since there's a fresh allocation above.
    // If it does, there's a bug.
    return TryAllocCommit(requirements, wanted_properties).value();
}

MemoryCommit MemoryAllocator::Commit(const vk::Buffer& buffer, bool host_visible) {
    auto commit = Commit(device.GetLogical().GetBufferMemoryRequirements(*buffer), host_visible);
    buffer.BindMemory(commit.Memory(), commit.Offset());
    return commit;
}

MemoryCommit MemoryAllocator::Commit(const vk::Image& image, bool host_visible) {
    auto commit = Commit(device.GetLogical().GetImageMemoryRequirements(*image), host_visible);
    image.BindMemory(commit.Memory(), commit.Offset());
    return commit;
}

void MemoryAllocator::AllocMemory(VkMemoryPropertyFlags wanted_properties, u32 type_mask,
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
    vk::DeviceMemory memory = device.GetLogical().AllocateMemory({
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .pNext = nullptr,
        .allocationSize = size,
        .memoryTypeIndex = type,
    });
    allocations.push_back(std::make_unique<MemoryAllocation>(device, std::move(memory),
                                                             wanted_properties, size, type));
}

std::optional<MemoryCommit> MemoryAllocator::TryAllocCommit(
    const VkMemoryRequirements& requirements, VkMemoryPropertyFlags wanted_properties) {
    for (auto& allocation : allocations) {
        if (!allocation->IsCompatible(wanted_properties, requirements.memoryTypeBits)) {
            continue;
        }
        if (auto commit = allocation->Commit(requirements.size, requirements.alignment)) {
            return commit;
        }
    }
    return std::nullopt;
}

} // namespace Vulkan
