// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <map>
#include <optional>
#include <vector>

#include "common/common_types.h"
#include "common/multi_level_page_table.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace Core {
class System;
}

namespace Tegra {

class MemoryManager final {
public:
    explicit MemoryManager(Core::System& system_, u64 address_space_bits_ = 40,
                           u64 page_bits_ = 16);
    ~MemoryManager();

    /// Binds a renderer to the memory manager.
    void BindRasterizer(VideoCore::RasterizerInterface* rasterizer);

    [[nodiscard]] std::optional<VAddr> GpuToCpuAddress(GPUVAddr addr) const;

    [[nodiscard]] std::optional<VAddr> GpuToCpuAddress(GPUVAddr addr, std::size_t size) const;

    template <typename T>
    [[nodiscard]] T Read(GPUVAddr addr) const;

    template <typename T>
    void Write(GPUVAddr addr, T data);

    [[nodiscard]] u8* GetPointer(GPUVAddr addr);
    [[nodiscard]] const u8* GetPointer(GPUVAddr addr) const;

    /**
     * ReadBlock and WriteBlock are full read and write operations over virtual
     * GPU Memory. It's important to use these when GPU memory may not be continuous
     * in the Host Memory counterpart. Note: This functions cause Host GPU Memory
     * Flushes and Invalidations, respectively to each operation.
     */
    void ReadBlock(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const;
    void WriteBlock(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size);
    void CopyBlock(GPUVAddr gpu_dest_addr, GPUVAddr gpu_src_addr, std::size_t size);

    /**
     * ReadBlockUnsafe and WriteBlockUnsafe are special versions of ReadBlock and
     * WriteBlock respectively. In this versions, no flushing or invalidation is actually
     * done and their performance is similar to a memcpy. This functions can be used
     * on either of this 2 scenarios instead of their safe counterpart:
     * - Memory which is sure to never be represented in the Host GPU.
     * - Memory Managed by a Cache Manager. Example: Texture Flushing should use
     * WriteBlockUnsafe instead of WriteBlock since it shouldn't invalidate the texture
     * being flushed.
     */
    void ReadBlockUnsafe(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size) const;
    void WriteBlockUnsafe(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size);

    /**
     * Checks if a gpu region can be simply read with a pointer.
     */
    [[nodiscard]] bool IsGranularRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Checks if a gpu region is mapped by a single range of cpu addresses.
     */
    [[nodiscard]] bool IsContinousRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Checks if a gpu region is mapped entirely.
     */
    [[nodiscard]] bool IsFullyMappedRange(GPUVAddr gpu_addr, std::size_t size) const;

    /**
     * Returns a vector with all the subranges of cpu addresses mapped beneath.
     * if the region is continous, a single pair will be returned. If it's unmapped, an empty vector
     * will be returned;
     */
    std::vector<std::pair<GPUVAddr, std::size_t>> GetSubmappedRange(GPUVAddr gpu_addr,
                                                                    std::size_t size) const;

    [[nodiscard]] GPUVAddr Map(VAddr cpu_addr, GPUVAddr gpu_addr, std::size_t size);
    [[nodiscard]] GPUVAddr MapAllocate(VAddr cpu_addr, std::size_t size, std::size_t align);
    [[nodiscard]] GPUVAddr MapAllocate32(VAddr cpu_addr, std::size_t size);
    [[nodiscard]] std::optional<GPUVAddr> AllocateFixed(GPUVAddr gpu_addr, std::size_t size);
    [[nodiscard]] GPUVAddr Allocate(std::size_t size, std::size_t align);
    void Unmap(GPUVAddr gpu_addr, std::size_t size);

    void FlushRegion(GPUVAddr gpu_addr, size_t size) const;

private:
    [[nodiscard]] std::optional<GPUVAddr> FindFreeRange(std::size_t size, std::size_t align,
                                                        bool start_32bit_address = false) const;

    void ReadBlockImpl(GPUVAddr gpu_src_addr, void* dest_buffer, std::size_t size,
                       bool is_safe) const;
    void WriteBlockImpl(GPUVAddr gpu_dest_addr, const void* src_buffer, std::size_t size,
                        bool is_safe);

    [[nodiscard]] inline std::size_t PageEntryIndex(GPUVAddr gpu_addr) const {
        return (gpu_addr >> page_bits) & page_table_mask;
    }

    Core::System& system;

    const u64 address_space_bits;
    const u64 page_bits;
    u64 address_space_size;
    u64 allocate_start;
    u64 page_size;
    u64 page_mask;
    u64 page_table_mask;
    static constexpr u64 cpu_page_bits{12};

    VideoCore::RasterizerInterface* rasterizer = nullptr;

    enum class EntryType : u64 {
        Free = 0,
        Reserved = 1,
        Mapped = 2,
    };

    std::vector<u64> entries;

    template <EntryType entry_type>
    GPUVAddr PageTableOp(GPUVAddr gpu_addr, [[maybe_unused]] VAddr cpu_addr, size_t size);

    EntryType GetEntry(size_t position) const;

    void SetEntry(size_t position, EntryType entry);

    Common::MultiLevelPageTable<u32> page_table;
};

} // namespace Tegra
