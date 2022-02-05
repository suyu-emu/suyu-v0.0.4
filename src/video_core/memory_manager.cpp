// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/device_memory.h"
#include "core/hle/kernel/k_page_table.h"
#include "core/hle/kernel/k_process.h"
#include "core/memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

#pragma optimize("", off)

namespace Tegra {

std::atomic<size_t> MemoryManager::unique_identifier_generator{};

MemoryManager::MemoryManager(Core::System& system_, u64 address_space_bits_, u64 big_page_bits_,
                             u64 page_bits_)
    : system{system_}, memory{system.Memory()}, device_memory{system.DeviceMemory()},
      address_space_bits{address_space_bits_}, page_bits{page_bits_}, big_page_bits{big_page_bits_},
      entries{}, big_entries{}, page_table{address_space_bits, address_space_bits + page_bits - 38,
                                           page_bits != big_page_bits ? page_bits : 0},
      unique_identifier{unique_identifier_generator.fetch_add(1, std::memory_order_acq_rel)} {
    address_space_size = 1ULL << address_space_bits;
    page_size = 1ULL << page_bits;
    page_mask = page_size - 1ULL;
    big_page_size = 1ULL << big_page_bits;
    big_page_mask = big_page_size - 1ULL;
    const u64 page_table_bits = address_space_bits - page_bits;
    const u64 big_page_table_bits = address_space_bits - big_page_bits;
    const u64 page_table_size = 1ULL << page_table_bits;
    const u64 big_page_table_size = 1ULL << big_page_table_bits;
    page_table_mask = page_table_size - 1;
    big_page_table_mask = big_page_table_size - 1;

    big_entries.resize(big_page_table_size / 32, 0);
    big_page_table_cpu.resize(big_page_table_size);
    big_page_table_physical.resize(big_page_table_size);
    entries.resize(page_table_size / 32, 0);
}

MemoryManager::~MemoryManager() = default;

template <bool is_big_page>
MemoryManager::EntryType MemoryManager::GetEntry(size_t position) const {
    if constexpr (is_big_page) {
        position = position >> big_page_bits;
        const u64 entry_mask = big_entries[position / 32];
        const size_t sub_index = position % 32;
        return static_cast<EntryType>((entry_mask >> (2 * sub_index)) & 0x03ULL);
    } else {
        position = position >> page_bits;
        const u64 entry_mask = entries[position / 32];
        const size_t sub_index = position % 32;
        return static_cast<EntryType>((entry_mask >> (2 * sub_index)) & 0x03ULL);
    }
}

template <bool is_big_page>
void MemoryManager::SetEntry(size_t position, MemoryManager::EntryType entry) {
    if constexpr (is_big_page) {
        position = position >> big_page_bits;
        const u64 entry_mask = big_entries[position / 32];
        const size_t sub_index = position % 32;
        big_entries[position / 32] =
            (~(3ULL << sub_index * 2) & entry_mask) | (static_cast<u64>(entry) << sub_index * 2);
    } else {
        position = position >> page_bits;
        const u64 entry_mask = entries[position / 32];
        const size_t sub_index = position % 32;
        entries[position / 32] =
            (~(3ULL << sub_index * 2) & entry_mask) | (static_cast<u64>(entry) << sub_index * 2);
    }
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
        [[maybe_unused]] const auto current_entry_type = GetEntry<false>(current_gpu_addr);
        SetEntry<false>(current_gpu_addr, entry_type);
        if (current_entry_type != entry_type) {
            rasterizer->ModifyGPUMemory(unique_identifier, gpu_addr, page_size);
        }
        if constexpr (entry_type == EntryType::Mapped) {
            const VAddr current_cpu_addr = cpu_addr + offset;
            const auto index = PageEntryIndex<false>(current_gpu_addr);
            const u32 sub_value = static_cast<u32>(current_cpu_addr >> cpu_page_bits);
            page_table[index] = sub_value;
        }
        remaining_size -= page_size;
    }
    return gpu_addr;
}

template <MemoryManager::EntryType entry_type>
GPUVAddr MemoryManager::BigPageTableOp(GPUVAddr gpu_addr, [[maybe_unused]] VAddr cpu_addr,
                                       size_t size) {
    u64 remaining_size{size};
    for (u64 offset{}; offset < size; offset += big_page_size) {
        const GPUVAddr current_gpu_addr = gpu_addr + offset;
        [[maybe_unused]] const auto current_entry_type = GetEntry<true>(current_gpu_addr);
        SetEntry<true>(current_gpu_addr, entry_type);
        if (current_entry_type != entry_type) {
            rasterizer->ModifyGPUMemory(unique_identifier, gpu_addr, big_page_size);
        }
        if constexpr (entry_type == EntryType::Mapped) {
            const VAddr current_cpu_addr = cpu_addr + offset;
            const auto index = PageEntryIndex<true>(current_gpu_addr);
            const u32 sub_value = static_cast<u32>(current_cpu_addr >> cpu_page_bits);
            big_page_table_cpu[index] = sub_value;
            const PAddr phys_address =
                device_memory.GetPhysicalAddr(memory.GetPointer(current_cpu_addr));
            big_page_table_physical[index] = static_cast<u32>(phys_address);
        }
        remaining_size -= big_page_size;
    }
    return gpu_addr;
}

void MemoryManager::BindRasterizer(VideoCore::RasterizerInterface* rasterizer_) {
    rasterizer = rasterizer_;
}

GPUVAddr MemoryManager::Map(GPUVAddr gpu_addr, VAddr cpu_addr, std::size_t size,
                            bool is_big_pages) {
    if (is_big_pages) [[likely]] {
        return BigPageTableOp<EntryType::Mapped>(gpu_addr, cpu_addr, size);
    }
    return PageTableOp<EntryType::Mapped>(gpu_addr, cpu_addr, size);
}

GPUVAddr MemoryManager::MapSparse(GPUVAddr gpu_addr, std::size_t size, bool is_big_pages) {
    if (is_big_pages) [[likely]] {
        return BigPageTableOp<EntryType::Reserved>(gpu_addr, 0, size);
    }
    return PageTableOp<EntryType::Reserved>(gpu_addr, 0, size);
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

    BigPageTableOp<EntryType::Free>(gpu_addr, 0, size);
    PageTableOp<EntryType::Free>(gpu_addr, 0, size);
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr gpu_addr) const {
    if (GetEntry<true>(gpu_addr) != EntryType::Mapped) [[unlikely]] {
        if (GetEntry<false>(gpu_addr) != EntryType::Mapped) {
            return std::nullopt;
        }

        const VAddr cpu_addr_base = static_cast<VAddr>(page_table[PageEntryIndex<false>(gpu_addr)])
                                    << cpu_page_bits;
        return cpu_addr_base + (gpu_addr & page_mask);
    }

    const VAddr cpu_addr_base =
        static_cast<VAddr>(big_page_table_cpu[PageEntryIndex<true>(gpu_addr)]) << cpu_page_bits;
    return cpu_addr_base + (gpu_addr & big_page_mask);
}

std::optional<VAddr> MemoryManager::GpuToCpuAddress(GPUVAddr addr, std::size_t size) const {
    size_t page_index{addr >> page_bits};
    const size_t page_last{(addr + size + page_size - 1) >> page_bits};
    while (page_index < page_last) {
        const auto page_addr{GpuToCpuAddress(page_index << page_bits)};
        if (page_addr) {
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

    return memory.GetPointer(*address);
}

const u8* MemoryManager::GetPointer(GPUVAddr gpu_addr) const {
    const auto address{GpuToCpuAddress(gpu_addr)};
    if (!address) {
        return {};
    }

    return memory.GetPointer(*address);
}

#pragma inline_recursion(on)

template <bool is_big_pages, typename FuncMapped, typename FuncReserved, typename FuncUnmapped>
inline void MemoryManager::MemoryOperation(GPUVAddr gpu_src_addr, std::size_t size,
                                           FuncMapped&& func_mapped, FuncReserved&& func_reserved,
                                           FuncUnmapped&& func_unmapped) const {
    u64 used_page_size;
    u64 used_page_mask;
    u64 used_page_bits;
    if constexpr (is_big_pages) {
        used_page_size = big_page_size;
        used_page_mask = big_page_mask;
        used_page_bits = big_page_bits;
    } else {
        used_page_size = page_size;
        used_page_mask = page_mask;
        used_page_bits = page_bits;
    }
    std::size_t remaining_size{size};
    std::size_t page_index{gpu_src_addr >> used_page_bits};
    std::size_t page_offset{gpu_src_addr & used_page_mask};
    GPUVAddr current_address = gpu_src_addr;

    while (remaining_size > 0) {
        const std::size_t copy_amount{
            std::min(static_cast<std::size_t>(used_page_size) - page_offset, remaining_size)};
        auto entry = GetEntry<is_big_pages>(current_address);
        if (entry == EntryType::Mapped) [[likely]] {
            func_mapped(page_index, page_offset, copy_amount);
        } else if (entry == EntryType::Reserved) {
            func_reserved(page_index, page_offset, copy_amount);
        } else [[unlikely]] {
            func_unmapped(page_index, page_offset, copy_amount);
        }
        page_index++;
        page_offset = 0;
        remaining_size -= copy_amount;
        current_address += copy_amount;
    }
}

template <bool is_safe>
void MemoryManager::ReadBlockImpl(GPUVAddr gpu_src_addr, void* dest_buffer,
                                  std::size_t size) const {
    auto set_to_zero = [&]([[maybe_unused]] std::size_t page_index,
                           [[maybe_unused]] std::size_t offset, std::size_t copy_amount) {
        std::memset(dest_buffer, 0, copy_amount);
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->FlushRegion(cpu_addr_base, copy_amount);
        }
        memory.ReadBlockUnsafe(cpu_addr_base, dest_buffer, copy_amount);
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(big_page_table_cpu[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->FlushRegion(cpu_addr_base, copy_amount);
        }
        memory.ReadBlockUnsafe(cpu_addr_base, dest_buffer, copy_amount);
        // u8* physical = device_memory.GetPointer(big_page_table_physical[page_index] + offset);
        // std::memcpy(dest_buffer, physical, copy_amount);
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
    };
    auto read_short_pages = [&](std::size_t page_index, std::size_t offset,
                                std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, set_to_zero, set_to_zero);
    };
    MemoryOperation<true>(gpu_src_addr, size, mapped_big, set_to_zero, read_short_pages);
}

void MemoryManager::ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const {
    ReadBlockImpl<true>(gpu_src_addr, dest_buffer, size);
}

void MemoryManager::ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer,
                                    const std::size_t size) const {
    ReadBlockImpl<false>(gpu_src_addr, dest_buffer, size);
}

template <bool is_safe>
void MemoryManager::WriteBlockImpl(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                   std::size_t size) {
    auto just_advance = [&]([[maybe_unused]] std::size_t page_index,
                            [[maybe_unused]] std::size_t offset, std::size_t copy_amount) {
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->InvalidateRegion(cpu_addr_base, copy_amount);
        }
        memory.WriteBlockUnsafe(cpu_addr_base, src_buffer, copy_amount);
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(big_page_table_cpu[page_index]) << cpu_page_bits) + offset;
        if constexpr (is_safe) {
            rasterizer->InvalidateRegion(cpu_addr_base, copy_amount);
        }
        memory.WriteBlockUnsafe(cpu_addr_base, src_buffer, copy_amount);
        /*u8* physical =
            device_memory.GetPointer(big_page_table_physical[page_index] << cpu_page_bits) + offset;
        std::memcpy(physical, src_buffer, copy_amount);*/
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
    };
    auto write_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, just_advance, just_advance);
    };
    MemoryOperation<true>(gpu_dest_addr, size, mapped_big, just_advance, write_short_pages);
}

void MemoryManager::WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size) {
    WriteBlockImpl<true>(gpu_dest_addr, src_buffer, size);
}

void MemoryManager::WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer,
                                     std::size_t size) {
    WriteBlockImpl<false>(gpu_dest_addr, src_buffer, size);
}

void MemoryManager::FlushRegion(GPUVAddr gpu_addr, size_t size) const {
    auto do_nothing = [&]([[maybe_unused]] std::size_t page_index,
                          [[maybe_unused]] std::size_t offset,
                          [[maybe_unused]] std::size_t copy_amount) {};

    auto mapped_normal = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        rasterizer->FlushRegion(cpu_addr_base, copy_amount);
    };
    auto mapped_big = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(big_page_table_cpu[page_index]) << cpu_page_bits) + offset;
        rasterizer->FlushRegion(cpu_addr_base, copy_amount);
    };
    auto flush_short_pages = [&](std::size_t page_index, std::size_t offset,
                                 std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, mapped_normal, do_nothing, do_nothing);
    };
    MemoryOperation<true>(gpu_addr, size, mapped_big, do_nothing, flush_short_pages);
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
    size_t page_index{gpu_addr >> big_page_bits};
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
        if (GetEntry<false>(page_index << page_bits) == EntryType::Free) {
            return false;
        }
        ++page_index;
    }
    return true;
}

#pragma inline_recursion(on)

std::vector<std::pair<GPUVAddr, std::size_t>> MemoryManager::GetSubmappedRange(
    GPUVAddr gpu_addr, std::size_t size) const {
    std::vector<std::pair<GPUVAddr, std::size_t>> result{};
    std::optional<std::pair<GPUVAddr, std::size_t>> last_segment{};
    std::optional<VAddr> old_page_addr{};
    const auto split = [&last_segment, &result]([[maybe_unused]] std::size_t page_index,
                                                [[maybe_unused]] std::size_t offset,
                                                [[maybe_unused]] std::size_t copy_amount) {
        if (last_segment) {
            result.push_back(*last_segment);
            last_segment = std::nullopt;
        }
    };
    const auto extend_size_big = [this, &split, &old_page_addr,
                                  &last_segment](std::size_t page_index, std::size_t offset,
                                                 std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(big_page_table_cpu[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr) {
            if (*old_page_addr != cpu_addr_base) {
                split(0, 0, 0);
            }
        }
        old_page_addr = {cpu_addr_base + copy_amount};
        if (!last_segment) {
            const GPUVAddr new_base_addr = (page_index << big_page_bits) + offset;
            last_segment = {new_base_addr, copy_amount};
        } else {
            last_segment->second += copy_amount;
        }
    };
    const auto extend_size_short = [this, &split, &old_page_addr,
                                    &last_segment](std::size_t page_index, std::size_t offset,
                                                   std::size_t copy_amount) {
        const VAddr cpu_addr_base =
            (static_cast<VAddr>(page_table[page_index]) << cpu_page_bits) + offset;
        if (old_page_addr) {
            if (*old_page_addr != cpu_addr_base) {
                split(0, 0, 0);
            }
        }
        old_page_addr = {cpu_addr_base + copy_amount};
        if (!last_segment) {
            const GPUVAddr new_base_addr = (page_index << page_bits) + offset;
            last_segment = {new_base_addr, copy_amount};
        } else {
            last_segment->second += copy_amount;
        }
    };
    auto do_short_pages = [&](std::size_t page_index, std::size_t offset, std::size_t copy_amount) {
        GPUVAddr base = (page_index << big_page_bits) + offset;
        MemoryOperation<false>(base, copy_amount, extend_size_short, split, split);
    };
    MemoryOperation<true>(gpu_addr, size, extend_size_big, split, do_short_pages);
    split(0, 0, 0);
    return result;
}

} // namespace Tegra
