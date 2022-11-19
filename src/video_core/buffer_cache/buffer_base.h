// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <bit>
#include <limits>
#include <utility>

#include "common/alignment.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "video_core/buffer_cache/word_manager.h"

namespace VideoCommon {

enum class BufferFlagBits {
    Picked = 1 << 0,
    CachedWrites = 1 << 1,
};
DECLARE_ENUM_FLAG_OPERATORS(BufferFlagBits)

/// Tag for creating null buffers with no storage or size
struct NullBufferParams {};

/**
 * Range tracking buffer container.
 *
 * It keeps track of the modified CPU and GPU ranges on a CPU page granularity, notifying the given
 * rasterizer about state changes in the tracking behavior of the buffer.
 *
 * The buffer size and address is forcefully aligned to CPU page boundaries.
 */
template <class RasterizerInterface>
class BufferBase {
public:
    static constexpr u64 BASE_PAGE_BITS = 16;
    static constexpr u64 BASE_PAGE_SIZE = 1ULL << BASE_PAGE_BITS;

    explicit BufferBase(RasterizerInterface& rasterizer_, VAddr cpu_addr_, u64 size_bytes)
        : cpu_addr{Common::AlignDown(cpu_addr_, BASE_PAGE_SIZE)},
          word_manager(cpu_addr, rasterizer_,
                       Common::AlignUp(size_bytes + (cpu_addr_ - cpu_addr), BASE_PAGE_SIZE)) {}

    explicit BufferBase(NullBufferParams) {}

    BufferBase& operator=(const BufferBase&) = delete;
    BufferBase(const BufferBase&) = delete;

    BufferBase& operator=(BufferBase&&) = default;
    BufferBase(BufferBase&&) = default;

    /// Returns the inclusive CPU modified range in a begin end pair
    [[nodiscard]] std::pair<u64, u64> ModifiedCpuRegion(VAddr query_cpu_addr,
                                                        u64 query_size) const noexcept {
        const u64 offset = query_cpu_addr - cpu_addr;
        return word_manager.ModifiedRegion<Type::CPU>(offset, query_size);
    }

    /// Returns the inclusive GPU modified range in a begin end pair
    [[nodiscard]] std::pair<u64, u64> ModifiedGpuRegion(VAddr query_cpu_addr,
                                                        u64 query_size) const noexcept {
        const u64 offset = query_cpu_addr - cpu_addr;
        return word_manager.ModifiedRegion<Type::GPU>(offset, query_size);
    }

    /// Returns true if a region has been modified from the CPU
    [[nodiscard]] bool IsRegionCpuModified(VAddr query_cpu_addr, u64 query_size) const noexcept {
        const u64 offset = query_cpu_addr - cpu_addr;
        return word_manager.IsRegionModified<Type::CPU>(offset, query_size);
    }

    /// Returns true if a region has been modified from the GPU
    [[nodiscard]] bool IsRegionGpuModified(VAddr query_cpu_addr, u64 query_size) const noexcept {
        const u64 offset = query_cpu_addr - cpu_addr;
        return word_manager.IsRegionModified<Type::GPU>(offset, query_size);
    }

    /// Mark region as CPU modified, notifying the rasterizer about this change
    void MarkRegionAsCpuModified(VAddr dirty_cpu_addr, u64 size) {
        word_manager.ChangeRegionState<Type::CPU, true>(dirty_cpu_addr, size);
    }

    /// Unmark region as CPU modified, notifying the rasterizer about this change
    void UnmarkRegionAsCpuModified(VAddr dirty_cpu_addr, u64 size) {
        word_manager.ChangeRegionState<Type::CPU, false>(dirty_cpu_addr, size);
    }

    /// Mark region as modified from the host GPU
    void MarkRegionAsGpuModified(VAddr dirty_cpu_addr, u64 size) noexcept {
        word_manager.ChangeRegionState<Type::GPU, true>(dirty_cpu_addr, size);
    }

    /// Unmark region as modified from the host GPU
    void UnmarkRegionAsGpuModified(VAddr dirty_cpu_addr, u64 size) noexcept {
        word_manager.ChangeRegionState<Type::GPU, false>(dirty_cpu_addr, size);
    }

    /// Mark region as modified from the CPU
    /// but don't mark it as modified until FlusHCachedWrites is called.
    void CachedCpuWrite(VAddr dirty_cpu_addr, u64 size) {
        flags |= BufferFlagBits::CachedWrites;
        word_manager.ChangeRegionState<Type::CachedCPU, true>(dirty_cpu_addr, size);
    }

    /// Flushes cached CPU writes, and notify the rasterizer about the deltas
    void FlushCachedWrites() noexcept {
        flags &= ~BufferFlagBits::CachedWrites;
        word_manager.FlushCachedWrites();
    }

    /// Call 'func' for each CPU modified range and unmark those pages as CPU modified
    template <typename Func>
    void ForEachUploadRange(VAddr query_cpu_range, u64 size, Func&& func) {
        word_manager.ForEachModifiedRange<Type::CPU>(query_cpu_range, size, true, func);
    }

    /// Call 'func' for each GPU modified range and unmark those pages as GPU modified
    template <typename Func>
    void ForEachDownloadRange(VAddr query_cpu_range, u64 size, bool clear, Func&& func) {
        word_manager.ForEachModifiedRange<Type::GPU>(query_cpu_range, size, clear, func);
    }

    template <typename Func>
    void ForEachDownloadRangeAndClear(VAddr query_cpu_range, u64 size, Func&& func) {
        word_manager.ForEachModifiedRange<Type::GPU>(query_cpu_range, size, true, func);
    }

    /// Call 'func' for each GPU modified range and unmark those pages as GPU modified
    template <typename Func>
    void ForEachDownloadRange(Func&& func) {
        word_manager.ForEachModifiedRange<Type::GPU>(cpu_addr, SizeBytes(), true, func);
    }

    /// Mark buffer as picked
    void Pick() noexcept {
        flags |= BufferFlagBits::Picked;
    }

    /// Unmark buffer as picked
    void Unpick() noexcept {
        flags &= ~BufferFlagBits::Picked;
    }

    /// Increases the likeliness of this being a stream buffer
    void IncreaseStreamScore(int score) noexcept {
        stream_score += score;
    }

    /// Returns the likeliness of this being a stream buffer
    [[nodiscard]] int StreamScore() const noexcept {
        return stream_score;
    }

    /// Returns true when vaddr -> vaddr+size is fully contained in the buffer
    [[nodiscard]] bool IsInBounds(VAddr addr, u64 size) const noexcept {
        return addr >= cpu_addr && addr + size <= cpu_addr + SizeBytes();
    }

    /// Returns true if the buffer has been marked as picked
    [[nodiscard]] bool IsPicked() const noexcept {
        return True(flags & BufferFlagBits::Picked);
    }

    /// Returns true when the buffer has pending cached writes
    [[nodiscard]] bool HasCachedWrites() const noexcept {
        return True(flags & BufferFlagBits::CachedWrites);
    }

    /// Returns the base CPU address of the buffer
    [[nodiscard]] VAddr CpuAddr() const noexcept {
        return cpu_addr;
    }

    /// Returns the offset relative to the given CPU address
    /// @pre IsInBounds returns true
    [[nodiscard]] u32 Offset(VAddr other_cpu_addr) const noexcept {
        return static_cast<u32>(other_cpu_addr - cpu_addr);
    }

    /// Returns the size in bytes of the buffer
    [[nodiscard]] u64 SizeBytes() const noexcept {
        return word_manager.SizeBytes();
    }

    size_t getLRUID() const noexcept {
        return lru_id;
    }

    void setLRUID(size_t lru_id_) {
        lru_id = lru_id_;
    }

private:
    VAddr cpu_addr = 0;
    WordManager<RasterizerInterface> word_manager;
    BufferFlagBits flags{};
    int stream_score = 0;
    size_t lru_id = SIZE_MAX;
};

} // namespace VideoCommon
