// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <boost/container/small_vector.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/intrusive/set.hpp>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/buffer_cache/buffer_block.h"
#include "video_core/buffer_cache/map_interval.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCommon {

template <typename Buffer, typename BufferType, typename StreamBuffer>
class BufferCache {
    using IntervalSet = boost::icl::interval_set<VAddr>;
    using IntervalType = typename IntervalSet::interval_type;
    using VectorMapInterval = boost::container::small_vector<MapInterval*, 1>;

    static constexpr u64 WRITE_PAGE_BIT = 11;
    static constexpr u64 BLOCK_PAGE_BITS = 21;
    static constexpr u64 BLOCK_PAGE_SIZE = 1ULL << BLOCK_PAGE_BITS;

public:
    struct BufferInfo {
        BufferType handle;
        u64 offset;
        u64 address;
    };

    BufferInfo UploadMemory(GPUVAddr gpu_addr, std::size_t size, std::size_t alignment = 4,
                            bool is_written = false, bool use_fast_cbuf = false) {
        std::lock_guard lock{mutex};

        auto& memory_manager = system.GPU().MemoryManager();
        const std::optional<VAddr> cpu_addr_opt = memory_manager.GpuToCpuAddress(gpu_addr);
        if (!cpu_addr_opt) {
            return GetEmptyBuffer(size);
        }
        const VAddr cpu_addr = *cpu_addr_opt;

        // Cache management is a big overhead, so only cache entries with a given size.
        // TODO: Figure out which size is the best for given games.
        constexpr std::size_t max_stream_size = 0x800;
        if (use_fast_cbuf || size < max_stream_size) {
            if (!is_written && !IsRegionWritten(cpu_addr, cpu_addr + size - 1)) {
                const bool is_granular = memory_manager.IsGranularRange(gpu_addr, size);
                if (use_fast_cbuf) {
                    u8* dest;
                    if (is_granular) {
                        dest = memory_manager.GetPointer(gpu_addr);
                    } else {
                        staging_buffer.resize(size);
                        dest = staging_buffer.data();
                        memory_manager.ReadBlockUnsafe(gpu_addr, dest, size);
                    }
                    return ConstBufferUpload(dest, size);
                }
                if (is_granular) {
                    u8* const host_ptr = memory_manager.GetPointer(gpu_addr);
                    return StreamBufferUpload(size, alignment, [host_ptr, size](u8* dest) {
                        std::memcpy(dest, host_ptr, size);
                    });
                } else {
                    return StreamBufferUpload(
                        size, alignment, [&memory_manager, gpu_addr, size](u8* dest) {
                            memory_manager.ReadBlockUnsafe(gpu_addr, dest, size);
                        });
                }
            }
        }

        Buffer* const block = GetBlock(cpu_addr, size);
        MapInterval* const map = MapAddress(block, gpu_addr, cpu_addr, size);
        if (!map) {
            return GetEmptyBuffer(size);
        }
        if (is_written) {
            map->MarkAsModified(true, GetModifiedTicks());
            if (Settings::IsGPULevelHigh() && Settings::values.use_asynchronous_gpu_emulation) {
                MarkForAsyncFlush(map);
            }
            if (!map->is_written) {
                map->is_written = true;
                MarkRegionAsWritten(map->start, map->end - 1);
            }
        }

        return BufferInfo{block->Handle(), block->Offset(cpu_addr), block->Address()};
    }

    /// Uploads from a host memory. Returns the OpenGL buffer where it's located and its offset.
    BufferInfo UploadHostMemory(const void* raw_pointer, std::size_t size,
                                std::size_t alignment = 4) {
        std::lock_guard lock{mutex};
        return StreamBufferUpload(size, alignment, [raw_pointer, size](u8* dest) {
            std::memcpy(dest, raw_pointer, size);
        });
    }

    /// Prepares the buffer cache for data uploading
    /// @param max_size Maximum number of bytes that will be uploaded
    /// @return True when a stream buffer invalidation was required, false otherwise
    bool Map(std::size_t max_size) {
        std::lock_guard lock{mutex};

        bool invalidated;
        std::tie(buffer_ptr, buffer_offset_base, invalidated) = stream_buffer->Map(max_size, 4);
        buffer_offset = buffer_offset_base;

        return invalidated;
    }

    /// Finishes the upload stream
    void Unmap() {
        std::lock_guard lock{mutex};
        stream_buffer->Unmap(buffer_offset - buffer_offset_base);
    }

    /// Function called at the end of each frame, inteded for deferred operations
    void TickFrame() {
        ++epoch;

        while (!pending_destruction.empty()) {
            // Delay at least 4 frames before destruction.
            // This is due to triple buffering happening on some drivers.
            static constexpr u64 epochs_to_destroy = 5;
            if (pending_destruction.front()->Epoch() + epochs_to_destroy > epoch) {
                break;
            }
            pending_destruction.pop();
        }
    }

    /// Write any cached resources overlapping the specified region back to memory
    void FlushRegion(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        VectorMapInterval objects = GetMapsInRange(addr, size);
        std::sort(objects.begin(), objects.end(),
                  [](MapInterval* lhs, MapInterval* rhs) { return lhs->ticks < rhs->ticks; });
        for (MapInterval* object : objects) {
            if (object->is_modified && object->is_registered) {
                mutex.unlock();
                FlushMap(object);
                mutex.lock();
            }
        }
    }

    bool MustFlushRegion(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        const VectorMapInterval objects = GetMapsInRange(addr, size);
        return std::any_of(objects.cbegin(), objects.cend(), [](const MapInterval* map) {
            return map->is_modified && map->is_registered;
        });
    }

    /// Mark the specified region as being invalidated
    void InvalidateRegion(VAddr addr, u64 size) {
        std::lock_guard lock{mutex};

        for (auto& object : GetMapsInRange(addr, size)) {
            if (object->is_registered) {
                Unregister(object);
            }
        }
    }

    void OnCPUWrite(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        for (MapInterval* object : GetMapsInRange(addr, size)) {
            if (object->is_memory_marked && object->is_registered) {
                UnmarkMemory(object);
                object->is_sync_pending = true;
                marked_for_unregister.emplace_back(object);
            }
        }
    }

    void SyncGuestHost() {
        std::lock_guard lock{mutex};

        for (auto& object : marked_for_unregister) {
            if (object->is_registered) {
                object->is_sync_pending = false;
                Unregister(object);
            }
        }
        marked_for_unregister.clear();
    }

    void CommitAsyncFlushes() {
        if (uncommitted_flushes) {
            auto commit_list = std::make_shared<std::list<MapInterval*>>();
            for (MapInterval* map : *uncommitted_flushes) {
                if (map->is_registered && map->is_modified) {
                    // TODO(Blinkhawk): Implement backend asynchronous flushing
                    // AsyncFlushMap(map)
                    commit_list->push_back(map);
                }
            }
            if (!commit_list->empty()) {
                committed_flushes.push_back(commit_list);
            } else {
                committed_flushes.emplace_back();
            }
        } else {
            committed_flushes.emplace_back();
        }
        uncommitted_flushes.reset();
    }

    bool ShouldWaitAsyncFlushes() const {
        return !committed_flushes.empty() && committed_flushes.front() != nullptr;
    }

    bool HasUncommittedFlushes() const {
        return uncommitted_flushes != nullptr;
    }

    void PopAsyncFlushes() {
        if (committed_flushes.empty()) {
            return;
        }
        auto& flush_list = committed_flushes.front();
        if (!flush_list) {
            committed_flushes.pop_front();
            return;
        }
        for (MapInterval* map : *flush_list) {
            if (map->is_registered) {
                // TODO(Blinkhawk): Replace this for reading the asynchronous flush
                FlushMap(map);
            }
        }
        committed_flushes.pop_front();
    }

    virtual BufferInfo GetEmptyBuffer(std::size_t size) = 0;

protected:
    explicit BufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                         std::unique_ptr<StreamBuffer> stream_buffer)
        : rasterizer{rasterizer}, system{system}, stream_buffer{std::move(stream_buffer)} {}

    ~BufferCache() = default;

    virtual std::shared_ptr<Buffer> CreateBlock(VAddr cpu_addr, std::size_t size) = 0;

    virtual BufferInfo ConstBufferUpload(const void* raw_pointer, std::size_t size) {
        return {};
    }

    /// Register an object into the cache
    MapInterval* Register(MapInterval new_map, bool inherit_written = false) {
        const VAddr cpu_addr = new_map.start;
        if (!cpu_addr) {
            LOG_CRITICAL(HW_GPU, "Failed to register buffer with unmapped gpu_address 0x{:016x}",
                         new_map.gpu_addr);
            return nullptr;
        }
        const std::size_t size = new_map.end - new_map.start;
        new_map.is_registered = true;
        rasterizer.UpdatePagesCachedCount(cpu_addr, size, 1);
        new_map.is_memory_marked = true;
        if (inherit_written) {
            MarkRegionAsWritten(new_map.start, new_map.end - 1);
            new_map.is_written = true;
        }
        MapInterval* const storage = mapped_addresses_allocator.Allocate();
        *storage = new_map;
        mapped_addresses.insert(*storage);
        return storage;
    }

    void UnmarkMemory(MapInterval* map) {
        if (!map->is_memory_marked) {
            return;
        }
        const std::size_t size = map->end - map->start;
        rasterizer.UpdatePagesCachedCount(map->start, size, -1);
        map->is_memory_marked = false;
    }

    /// Unregisters an object from the cache
    void Unregister(MapInterval* map) {
        UnmarkMemory(map);
        map->is_registered = false;
        if (map->is_sync_pending) {
            map->is_sync_pending = false;
            marked_for_unregister.remove(map);
        }
        if (map->is_written) {
            UnmarkRegionAsWritten(map->start, map->end - 1);
        }
        const auto it = mapped_addresses.find(*map);
        ASSERT(it != mapped_addresses.end());
        mapped_addresses.erase(it);
        mapped_addresses_allocator.Release(map);
    }

private:
    MapInterval* MapAddress(const Buffer* block, GPUVAddr gpu_addr, VAddr cpu_addr,
                            std::size_t size) {
        const VectorMapInterval overlaps = GetMapsInRange(cpu_addr, size);
        if (overlaps.empty()) {
            auto& memory_manager = system.GPU().MemoryManager();
            const VAddr cpu_addr_end = cpu_addr + size;
            if (memory_manager.IsGranularRange(gpu_addr, size)) {
                u8* host_ptr = memory_manager.GetPointer(gpu_addr);
                block->Upload(block->Offset(cpu_addr), size, host_ptr);
            } else {
                staging_buffer.resize(size);
                memory_manager.ReadBlockUnsafe(gpu_addr, staging_buffer.data(), size);
                block->Upload(block->Offset(cpu_addr), size, staging_buffer.data());
            }
            return Register(MapInterval(cpu_addr, cpu_addr_end, gpu_addr));
        }

        const VAddr cpu_addr_end = cpu_addr + size;
        if (overlaps.size() == 1) {
            MapInterval* const current_map = overlaps[0];
            if (current_map->IsInside(cpu_addr, cpu_addr_end)) {
                return current_map;
            }
        }
        VAddr new_start = cpu_addr;
        VAddr new_end = cpu_addr_end;
        bool write_inheritance = false;
        bool modified_inheritance = false;
        // Calculate new buffer parameters
        for (MapInterval* overlap : overlaps) {
            new_start = std::min(overlap->start, new_start);
            new_end = std::max(overlap->end, new_end);
            write_inheritance |= overlap->is_written;
            modified_inheritance |= overlap->is_modified;
        }
        GPUVAddr new_gpu_addr = gpu_addr + new_start - cpu_addr;
        for (auto& overlap : overlaps) {
            Unregister(overlap);
        }
        UpdateBlock(block, new_start, new_end, overlaps);

        const MapInterval new_map{new_start, new_end, new_gpu_addr};
        MapInterval* const map = Register(new_map, write_inheritance);
        if (!map) {
            return nullptr;
        }
        if (modified_inheritance) {
            map->MarkAsModified(true, GetModifiedTicks());
            if (Settings::IsGPULevelHigh() && Settings::values.use_asynchronous_gpu_emulation) {
                MarkForAsyncFlush(map);
            }
        }
        return map;
    }

    void UpdateBlock(const Buffer* block, VAddr start, VAddr end,
                     const VectorMapInterval& overlaps) {
        const IntervalType base_interval{start, end};
        IntervalSet interval_set{};
        interval_set.add(base_interval);
        for (auto& overlap : overlaps) {
            const IntervalType subtract{overlap->start, overlap->end};
            interval_set.subtract(subtract);
        }
        for (auto& interval : interval_set) {
            const std::size_t size = interval.upper() - interval.lower();
            if (size == 0) {
                continue;
            }
            staging_buffer.resize(size);
            system.Memory().ReadBlockUnsafe(interval.lower(), staging_buffer.data(), size);
            block->Upload(block->Offset(interval.lower()), size, staging_buffer.data());
        }
    }

    VectorMapInterval GetMapsInRange(VAddr addr, std::size_t size) {
        VectorMapInterval result;
        if (size == 0) {
            return result;
        }

        const VAddr addr_end = addr + size;
        auto it = mapped_addresses.lower_bound(addr);
        if (it != mapped_addresses.begin()) {
            --it;
        }
        while (it != mapped_addresses.end() && it->start < addr_end) {
            if (it->Overlaps(addr, addr_end)) {
                result.push_back(&*it);
            }
            ++it;
        }
        return result;
    }

    /// Returns a ticks counter used for tracking when cached objects were last modified
    u64 GetModifiedTicks() {
        return ++modified_ticks;
    }

    void FlushMap(MapInterval* map) {
        const auto it = blocks.find(map->start >> BLOCK_PAGE_BITS);
        ASSERT_OR_EXECUTE(it != blocks.end(), return;);

        std::shared_ptr<Buffer> block = it->second;

        const std::size_t size = map->end - map->start;
        staging_buffer.resize(size);
        block->Download(block->Offset(map->start), size, staging_buffer.data());
        system.Memory().WriteBlockUnsafe(map->start, staging_buffer.data(), size);
        map->MarkAsModified(false, 0);
    }

    template <typename Callable>
    BufferInfo StreamBufferUpload(std::size_t size, std::size_t alignment, Callable&& callable) {
        AlignBuffer(alignment);
        const std::size_t uploaded_offset = buffer_offset;
        callable(buffer_ptr);

        buffer_ptr += size;
        buffer_offset += size;
        return BufferInfo{stream_buffer->Handle(), uploaded_offset, stream_buffer->Address()};
    }

    void AlignBuffer(std::size_t alignment) {
        // Align the offset, not the mapped pointer
        const std::size_t offset_aligned = Common::AlignUp(buffer_offset, alignment);
        buffer_ptr += offset_aligned - buffer_offset;
        buffer_offset = offset_aligned;
    }

    std::shared_ptr<Buffer> EnlargeBlock(std::shared_ptr<Buffer> buffer) {
        const std::size_t old_size = buffer->Size();
        const std::size_t new_size = old_size + BLOCK_PAGE_SIZE;
        const VAddr cpu_addr = buffer->CpuAddr();
        std::shared_ptr<Buffer> new_buffer = CreateBlock(cpu_addr, new_size);
        new_buffer->CopyFrom(*buffer, 0, 0, old_size);
        QueueDestruction(std::move(buffer));

        const VAddr cpu_addr_end = cpu_addr + new_size - 1;
        const u64 page_end = cpu_addr_end >> BLOCK_PAGE_BITS;
        for (u64 page_start = cpu_addr >> BLOCK_PAGE_BITS; page_start <= page_end; ++page_start) {
            blocks.insert_or_assign(page_start, new_buffer);
        }

        return new_buffer;
    }

    std::shared_ptr<Buffer> MergeBlocks(std::shared_ptr<Buffer> first,
                                        std::shared_ptr<Buffer> second) {
        const std::size_t size_1 = first->Size();
        const std::size_t size_2 = second->Size();
        const VAddr first_addr = first->CpuAddr();
        const VAddr second_addr = second->CpuAddr();
        const VAddr new_addr = std::min(first_addr, second_addr);
        const std::size_t new_size = size_1 + size_2;

        std::shared_ptr<Buffer> new_buffer = CreateBlock(new_addr, new_size);
        new_buffer->CopyFrom(*first, 0, new_buffer->Offset(first_addr), size_1);
        new_buffer->CopyFrom(*second, 0, new_buffer->Offset(second_addr), size_2);
        QueueDestruction(std::move(first));
        QueueDestruction(std::move(second));

        const VAddr cpu_addr_end = new_addr + new_size - 1;
        const u64 page_end = cpu_addr_end >> BLOCK_PAGE_BITS;
        for (u64 page_start = new_addr >> BLOCK_PAGE_BITS; page_start <= page_end; ++page_start) {
            blocks.insert_or_assign(page_start, new_buffer);
        }
        return new_buffer;
    }

    Buffer* GetBlock(VAddr cpu_addr, std::size_t size) {
        std::shared_ptr<Buffer> found;

        const VAddr cpu_addr_end = cpu_addr + size - 1;
        const u64 page_end = cpu_addr_end >> BLOCK_PAGE_BITS;
        for (u64 page_start = cpu_addr >> BLOCK_PAGE_BITS; page_start <= page_end; ++page_start) {
            auto it = blocks.find(page_start);
            if (it == blocks.end()) {
                if (found) {
                    found = EnlargeBlock(found);
                    continue;
                }
                const VAddr start_addr = page_start << BLOCK_PAGE_BITS;
                found = CreateBlock(start_addr, BLOCK_PAGE_SIZE);
                blocks.insert_or_assign(page_start, found);
                continue;
            }
            if (!found) {
                found = it->second;
                continue;
            }
            if (found != it->second) {
                found = MergeBlocks(std::move(found), it->second);
            }
        }
        return found.get();
    }

    void MarkRegionAsWritten(VAddr start, VAddr end) {
        const u64 page_end = end >> WRITE_PAGE_BIT;
        for (u64 page_start = start >> WRITE_PAGE_BIT; page_start <= page_end; ++page_start) {
            auto it = written_pages.find(page_start);
            if (it != written_pages.end()) {
                it->second = it->second + 1;
            } else {
                written_pages.insert_or_assign(page_start, 1);
            }
        }
    }

    void UnmarkRegionAsWritten(VAddr start, VAddr end) {
        const u64 page_end = end >> WRITE_PAGE_BIT;
        for (u64 page_start = start >> WRITE_PAGE_BIT; page_start <= page_end; ++page_start) {
            auto it = written_pages.find(page_start);
            if (it != written_pages.end()) {
                if (it->second > 1) {
                    it->second = it->second - 1;
                } else {
                    written_pages.erase(it);
                }
            }
        }
    }

    bool IsRegionWritten(VAddr start, VAddr end) const {
        const u64 page_end = end >> WRITE_PAGE_BIT;
        for (u64 page_start = start >> WRITE_PAGE_BIT; page_start <= page_end; ++page_start) {
            if (written_pages.count(page_start) > 0) {
                return true;
            }
        }
        return false;
    }

    void QueueDestruction(std::shared_ptr<Buffer> buffer) {
        buffer->SetEpoch(epoch);
        pending_destruction.push(std::move(buffer));
    }

    void MarkForAsyncFlush(MapInterval* map) {
        if (!uncommitted_flushes) {
            uncommitted_flushes = std::make_shared<std::unordered_set<MapInterval*>>();
        }
        uncommitted_flushes->insert(map);
    }

    VideoCore::RasterizerInterface& rasterizer;
    Core::System& system;

    std::unique_ptr<StreamBuffer> stream_buffer;
    BufferType stream_buffer_handle;

    u8* buffer_ptr = nullptr;
    u64 buffer_offset = 0;
    u64 buffer_offset_base = 0;

    MapIntervalAllocator mapped_addresses_allocator;
    boost::intrusive::set<MapInterval, boost::intrusive::compare<MapIntervalCompare>>
        mapped_addresses;

    std::unordered_map<u64, u32> written_pages;
    std::unordered_map<u64, std::shared_ptr<Buffer>> blocks;

    std::queue<std::shared_ptr<Buffer>> pending_destruction;
    u64 epoch = 0;
    u64 modified_ticks = 0;

    std::vector<u8> staging_buffer;

    std::list<MapInterval*> marked_for_unregister;

    std::shared_ptr<std::unordered_set<MapInterval*>> uncommitted_flushes;
    std::list<std::shared_ptr<std::list<MapInterval*>>> committed_flushes;

    std::recursive_mutex mutex;
};

} // namespace VideoCommon
