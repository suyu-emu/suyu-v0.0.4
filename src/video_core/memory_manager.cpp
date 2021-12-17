// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

namespace Tegra {

std::atomic<size_t> MemoryManager::unique_identifier_generator{};

MemoryManager::MemoryManager(Core::System& system_, u64 address_space_bits_, u64 page_bits_)
    : system{system_}, address_space_bits{address_space_bits_}, page_bits{page_bits_}, entries{},
      page_table{address_space_bits, address_space_bits + page_bits - 38, page_bits},
      unique_identifier{unique_identifier_generator.fetch_add(1, std::memory_order_acq_rel)} {
    address_space_size = 1ULL << address_space_bits;
    allocate_start = address_space_bits > 32 ? 1ULL << 32 : 0;
    page_size = 1ULL << page_bits;
    page_mask = page_size - 1ULL;
    const u64 page_table_bits = address_space_bits - cpu_page_bits;
    const u64 page_table_size = 1ULL << page_table_bits;
    page_table_mask = page_table_size - 1;

    entries.resize(page_table_size / 32, 0);
}

MemoryManager::~MemoryManager() = default;

MemoryManager::EntryType MemoryManager::GetEntry(size_t position) const {
    position = position >> page_bits;
    const u64 entry_mask = entries[position / 32];
    const size_t sub_index = position % 32;
    return static_cast<EntryType>((entry_mask >> (2 * sub_index)) & 0x03ULL);
}

void MemoryManager::SetEntry(size_t position, MemoryManager::EntryType entry) {
    position = position >> page_bits;
    const u64 entry_mask = entries[position / 32];
    const size_t sub_index = position % 32;
    entries[position / 32] =
        (~(3ULL << sub_index * 2) & entry_mask) | (static_cast<u64>(entry) << sub_index * 2);
}

template <MemoryManager::EntryType entry_type>
GPUVAddr MemoryManager::PageTableOp(GPUVAddr gpu_addr, [[maybe_unused]] VAddr cpu_addr,
                                    size_t size) {
    u64 remaining_size{size};
    if constexpr (entry_type == EntryType::Mapped) {
        page_table.ReserveRange(gpu_addr, size);
    }
    for (u64 offset{}; offset < size; offset += page_size) {
        const GPUVAddr current_gpu_addr = gpu_addr + offset;
        SetEntry(current_gpu_addr, entry_type);
        if constexpr (entry_type == EntryType::Mapped) {
            const VAddr current_cpu_addr = cpu_addr + offset;
            const auto index = PageEntryIndex(current_gpu_addr);
            page_table[index] = static_cast<u32>(current_cpu_addr >> 12ULL);
        }
        remaining_size -= page_size;
    }
    return gpu_addr;
}

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::Map(GPUVAddr gpu_addr, VAddr cpu_addr, std::size_t size) {
    return PageTableOp<EntryType::Mapped>(gpu_addr, cpu_addr, size);
}

GPUVAddr MemoryManager::MapSparse(GPUVAddr gpu_addr, std::size_t size) {
    return PageTableOp<EntryType::Reserved>(gpu_addr, 0, size);
}

GPUVAddr MemoryManager::MapAllocate(VAddr cpu_addr, std::size_t size, std::size_t align) {
    return Map(*FindFreeRange(size, align), cpu_addr, size);
}

GPUVAddr MemoryManager::MapAllocate32(VAddr cpu_addr, std::size_t size) {
    const std::optional<GPUVAddr> gpu_addr = FindFreeRange(size, 1, true);
    ASSERT(gpu_addr);
    return Map(*gpu_addr, cpu_addr, size);
}

void MemoryManager::Unmap(GPUVAddr gpu_addr, std::size_t size) {
    if (size == 0) {
        return;
    }
    const auto submapped_ranges = GetSubmappedRange(gpu_addr, size);

    for (const auto& [map_addr, map_size] : submapped_ranges) {
        // Flush and invalidate through the GPU interface, to be asynchronous if possible.
        const std::optional<VAddr> cpu_addr = GpuToCpuAddress(map_addr);
        ASSERT(cpu_addr);

        rasterizer->UnmapMemory(*cpu_addr, map_size);
    }

    PageTableOp<EntryType::Free>(gpu_addr, 0, size);
}

std::optional<GPUVAddr> MemoryManager::AllocateFixed(GPUVAddr gpu_addr, std::size_t size) {
    for (u64 offset{}; offset < size; offset += page_size) {
        if (GetEntry(gpu_addr + offset) != EntryType::Free) {
            return std::nullopt;
        }
    }

    return PageTableOp<EntryType::Reserved>(gpu_addr, 0, size);
}

GPUVAddr MemoryManager::Allocate(std::size_t size, std::size_t align) {
    return *AllocateFixed(*FindFreeRange(size, align), size);
}

std::optional<GPUVAddr> MemoryManager::FindFreeRange(std::size_t size, std::size_t align,
                                                     bool start_32bit_address) const {
    if (!align) {
        align = page_size;
    } else {
        align = Common::AlignUp(align, page_size);
    }

    u64 available_size{};
    GPUVAddr gpu_addr{start_32bit_address ? 0 : allocate_start};
    while (gpu_addr + available_size < address_space_size) {
        if (GetEntry(gpu_addr + available_size) == EntryType::Free) {
            available_size += page_size;

            if (available_size >= size) {
                return gpu_addr;
            }
        } else {
            gpu_addr += available_size + page_size;
            available_size = 0;

            const auto remainder{gpu_addr % align};
            if (remainder) {
                gpu_addr = (gpu_addr - remainder) + align;
            }
        }
    }

    return std::nullopt;
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr gpu_addr) const {
    if (GetEntry(gpu_addr) != EntryType::Mapped) {
        return std::nullopt;
    }

    const VAddr cpu_addr_base = static_cast<VAddr>(page_table[PageEntryIndex(gpu_addr)]) << 12ULL;
    return cpu_addr_base + (gpu_addr & page_mask);
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr addr, std::size_t size) const {
    size_t page_index{addr >> page_bits};
    const size_t page_last{(addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr && *page_addr != 0) {
            return page_addr;
        }
        ++page_index;
    }
    return std::nullopt;
}

template <typename T>
T MemoryManager::Read(GPUVAddr addr) const {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        T value;
        std::memcpy(&value, page_pointer, sizeof(T));
        return value;
    }

    ASSERT(false);

    return {};
}

template <typename T>
void MemoryManager::Write(GPUVAddr addr, T data) {
    if (auto page_pointer{GetPointer(addr)}; page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(page_pointer, &data, sizeof(T));
        return;
    }

    ASSERT(false);
}

template u8 MemoryManager::Read<u8>(GPUVAddr addr) const;
template u16 MemoryManager::Read<u16>(GPUVAddr addr) const;
template u32 MemoryManager::Read<u32>(GPUVAddr addr) const;
template u64 MemoryManager::Read<u64>(GPUVAddr addr) const;
template void MemoryManager::Write<u8>(GPUVAddr addr, u8 data);
template void MemoryManager::Write<u16>(GPUVAddr addr, u16 data);
template void MemoryManager::Write<u32>(GPUVAddr addr, u32 data);
template void MemoryManager::Write<u64>(GPUVAddr addr, u64 data);

u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) {
    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return system.Memory().GetPointer(*address);
}

const u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) const {
    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return system.Memory().GetPointer(*address);
}

void MemoryManager::ReadBlockImpl(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size,
                                  bool is_safe) const {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_src_addr >> page_bits};
    std::size_t page_offset{gpu_src_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr) {
            const auto src_addr{*page_addr + page_offset};
            if (is_safe) {
                // Flush must happen on the rasterizer interface, such that memory is always
                // synchronous when it is read (even when in asynchronous GPU mode).
                // Fixes Dead Cells title menu.
                rasterizer->FlushRegion(src_addr, copy_amount);
            }
            system.Memory().ReadBlockUnsafe(src_addr, dest_buffer, copy_amount);
        } else {
            std::memset(dest_buffer, 0, copy_amount);
        }

        page_index++;
        page_offset = 0;
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const {
    ReadBlockImpl(gpu_src_addr, dest_buffer, size, true);
}

void MemoryManager::ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer,
                                    const std::size_t size) const {
    ReadBlockImpl(gpu_src_addr, dest_buffer, size, false);
}

void MemoryManager::WriteBlockImpl(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size,
                                   bool is_safe) {
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_dest_addr >> page_bits};
    std::size_t page_offset{gpu_dest_addr & page_mask};

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(page_size) - page_offset, remaining_size)};
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr) {
            const auto dest_addr{*page_addr + page_offset};

            if (is_safe) {
                // Invalidate must happen on the rasterizer interface, such that memory is always
                // synchronous when it is written (even when in asynchronous GPU mode).
                rasterizer->InvalidateRegion(dest_addr, copy_amount);
            }
            system.Memory().WriteBlockUnsafe(dest_addr, src_buffer, copy_amount);
        }

        page_index++;
        page_offset = 0;
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void MemoryManager::WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size) {
    WriteBlockImpl(gpu_dest_addr, src_buffer, size, true);
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    WriteBlockImpl(gpu_dest_addr, src_buffer, size, false);
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size) const {
    size_t remaining_size{size};
    size_t page_index{gpu_addr >> page_bits};
    size_t page_offset{gpu_addr & page_mask};
    while (remaining_size > 0) {
        const size_t num_bytes{std::min(page_size - page_offset, remaining_size)};
        if (const auto page_addr{GpuToCpuAddress(page_index << page_bits)}; page_addr) {
            rasterizer->FlushRegion(*page_addr + page_offset, num_bytes);
        }
        ++page_index;
        page_offset = 0;
        remaining_size -= num_bytes;
    }
}

void MemoryManager::CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size) {
    std::vector<u8> tmp_buffer(size);
    ReadBlock(gpu_src_addr, tmp_buffer.data(), size);

    // The output block must be flushed in case it has data modified from the GPU.
    // Fixes NPC geometry in Zombie Panic in Wonderland DX
    FlushRegion(gpu_dest_addr, size);
    WriteBlock(gpu_dest_addr, tmp_buffer.data(), size);
}

bool MemoryManager::IsGranularRange(GPUVAddr gpu_addr, std::size_t size) const {
    const auto cpu_addr{GpuToCpuAddress(gpu_addr)};
    if (!cpu_addr) {
        return false;
    }
    const std::size_t page{(*cpu_addr & Core::Memory::YUZU_PAGEMASK) + size};
    return page <= Core::Memory::YUZU_PAGESIZE;
}

bool MemoryManager::IsContinousRange(GPUVAddr gpu_addr, std::size_t size) const {
    size_t page_index{gpu_addr >> page_bits};
    const size_t page_last{(gpu_addr + size + page_size - 1) >> page_bits};
    std::optional<VAddr> old_page_addr{};
    while (page_index != page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (!page_addr || *page_addr == 0) {
            return false;
        }
        if (old_page_addr) {
            if (*old_page_addr + page_size != *page_addr) {
                return false;
            }
        }
        old_page_addr = page_addr;
        ++page_index;
    }
    return true;
}

bool MemoryManager::IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const {
    size_t page_index{gpu_addr >> page_bits};
    const size_t page_last{(gpu_addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        if (GetEntry(page_index << page_bits) == EntryType::Free) {
            return false;
        }
        ++page_index;
    }
    return true;
}

std::vector<std::pair<GPUVAddr, std::size_t>> MemoryManager::GetSubmappedRange(
    GPUVAddr gpu_addr, std::size_t size) const {
    std::vector<std::pair<GPUVAddr, std::size_t>> result{};
    size_t page_index{gpu_addr >> page_bits};
    size_t remaining_size{size};
    size_t page_offset{gpu_addr & page_mask};
    std::optional<std::pair<GPUVAddr, std::size_t>> last_segment{};
    std::optional<VAddr> old_page_addr{};
    const auto extend_size = [this, &last_segment, &page_index, &page_offset](std::size_t bytes) {
        if (!last_segment) {
            const GPUVAddr new_base_addr = (page_index << page_bits) + page_offset;
            last_segment = {new_base_addr, bytes};
        } else {
            last_segment->second += bytes;
        }
    };
    const auto split = [&last_segment, &result] {
        if (last_segment) {
            result.push_back(*last_segment);
            last_segment = std::nullopt;
        }
    };
    while (remaining_size > 0) {
        const size_t num_bytes{std::min(page_size - page_offset, remaining_size)};
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (!page_addr || *page_addr == 0) {
            split();
        } else if (old_page_addr) {
            if (*old_page_addr + page_size != *page_addr) {
                split();
            }
            extend_size(num_bytes);
        } else {
            extend_size(num_bytes);
        }
        ++page_index;
        page_offset = 0;
        remaining_size -= num_bytes;
        old_page_addr = page_addr;
    }
    split();
    return result;
}

} // namespace Tegra
