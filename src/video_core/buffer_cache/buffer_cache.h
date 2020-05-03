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

#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/range/iterator_range.hpp>

#include "common/alignment.h"
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

using MapInterval = std::shared_ptr<MapIntervalBase>;

template <typename OwnerBuffer, typename BufferType, typename StreamBuffer>
class BufferCache {
public:
    using BufferInfo = std::pair<BufferType, u64>;

    BufferInfo UploadMemory(GPUVAddr gpu_addr, std::size_t size, std::size_t alignment = 4,
                            bool is_written = false, bool use_fast_cbuf = false) {
        std::lock_guard lock{mutex};

        const std::optional<VAddr> cpu_addr_opt =
            system.GPU().MemoryManager().GpuToCpuAddress(gpu_addr);

        if (!cpu_addr_opt) {
            return {GetEmptyBuffer(size), 0};
        }

        VAddr cpu_addr = *cpu_addr_opt;

        // Cache management is a big overhead, so only cache entries with a given size.
        // TODO: Figure out which size is the best for given games.
        constexpr std::size_t max_stream_size = 0x800;
        if (use_fast_cbuf || size < max_stream_size) {
            if (!is_written && !IsRegionWritten(cpu_addr, cpu_addr + size - 1)) {
                auto& memory_manager = system.GPU().MemoryManager();
                if (use_fast_cbuf) {
                    if (memory_manager.IsGranularRange(gpu_addr, size)) {
                        const auto host_ptr = memory_manager.GetPointer(gpu_addr);
                        return ConstBufferUpload(host_ptr, size);
                    } else {
                        staging_buffer.resize(size);
                        memory_manager.ReadBlockUnsafe(gpu_addr, staging_buffer.data(), size);
                        return ConstBufferUpload(staging_buffer.data(), size);
                    }
                } else {
                    if (memory_manager.IsGranularRange(gpu_addr, size)) {
                        const auto host_ptr = memory_manager.GetPointer(gpu_addr);
                        return StreamBufferUpload(host_ptr, size, alignment);
                    } else {
                        staging_buffer.resize(size);
                        memory_manager.ReadBlockUnsafe(gpu_addr, staging_buffer.data(), size);
                        return StreamBufferUpload(staging_buffer.data(), size, alignment);
                    }
                }
            }
        }

        auto block = GetBlock(cpu_addr, size);
        auto map = MapAddress(block, gpu_addr, cpu_addr, size);
        if (is_written) {
            map->MarkAsModified(true, GetModifiedTicks());
            if (Settings::IsGPULevelHigh() && Settings::values.use_asynchronous_gpu_emulation) {
                MarkForAsyncFlush(map);
            }
            if (!map->IsWritten()) {
                map->MarkAsWritten(true);
                MarkRegionAsWritten(map->GetStart(), map->GetEnd() - 1);
            }
        }

        return {ToHandle(block), static_cast<u64>(block->GetOffset(cpu_addr))};
    }

    /// Uploads from a host memory. Returns the OpenGL buffer where it's located and its offset.
    BufferInfo UploadHostMemory(const void* raw_pointer, std::size_t size,
                                std::size_t alignment = 4) {
        std::lock_guard lock{mutex};
        return StreamBufferUpload(raw_pointer, size, alignment);
    }

    void Map(std::size_t max_size) {
        std::lock_guard lock{mutex};

        std::tie(buffer_ptr, buffer_offset_base, invalidated) = stream_buffer->Map(max_size, 4);
        buffer_offset = buffer_offset_base;
    }

    /// Finishes the upload stream, returns true on bindings invalidation.
    bool Unmap() {
        std::lock_guard lock{mutex};

        stream_buffer->Unmap(buffer_offset - buffer_offset_base);
        return std::exchange(invalidated, false);
    }

    void TickFrame() {
        ++epoch;
        while (!pending_destruction.empty()) {
            // Delay at least 4 frames before destruction.
            // This is due to triple buffering happening on some drivers.
            static constexpr u64 epochs_to_destroy = 5;
            if (pending_destruction.front()->GetEpoch() + epochs_to_destroy > epoch) {
                break;
            }
            pending_destruction.pop_front();
        }
    }

    /// Write any cached resources overlapping the specified region back to memory
    void FlushRegion(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        std::vector<MapInterval> objects = GetMapsInRange(addr, size);
        std::sort(objects.begin(), objects.end(), [](const MapInterval& a, const MapInterval& b) {
            return a->GetModificationTick() < b->GetModificationTick();
        });
        for (auto& object : objects) {
            if (object->IsModified() && object->IsRegistered()) {
                mutex.unlock();
                FlushMap(object);
                mutex.lock();
            }
        }
    }

    bool MustFlushRegion(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        const std::vector<MapInterval> objects = GetMapsInRange(addr, size);
        return std::any_of(objects.cbegin(), objects.cend(), [](const MapInterval& map) {
            return map->IsModified() && map->IsRegistered();
        });
    }

    /// Mark the specified region as being invalidated
    void InvalidateRegion(VAddr addr, u64 size) {
        std::lock_guard lock{mutex};

        std::vector<MapInterval> objects = GetMapsInRange(addr, size);
        for (auto& object : objects) {
            if (object->IsRegistered()) {
                Unregister(object);
            }
        }
    }

    void OnCPUWrite(VAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        for (const auto& object : GetMapsInRange(addr, size)) {
            if (object->IsMemoryMarked() && object->IsRegistered()) {
                UnmarkMemory(object);
                object->SetSyncPending(true);
                marked_for_unregister.emplace_back(object);
            }
        }
    }

    void SyncGuestHost() {
        std::lock_guard lock{mutex};

        for (const auto& object : marked_for_unregister) {
            if (object->IsRegistered()) {
                object->SetSyncPending(false);
                Unregister(object);
            }
        }
        marked_for_unregister.clear();
    }

    void CommitAsyncFlushes() {
        if (uncommitted_flushes) {
            auto commit_list = std::make_shared<std::list<MapInterval>>();
            for (auto& map : *uncommitted_flushes) {
                if (map->IsRegistered() && map->IsModified()) {
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
        for (MapInterval& map : *flush_list) {
            if (map->IsRegistered()) {
                // TODO(Blinkhawk): Replace this for reading the asynchronous flush
                FlushMap(map);
            }
        }
        committed_flushes.pop_front();
    }

    virtual BufferType GetEmptyBuffer(std::size_t size) = 0;

protected:
    explicit BufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                         std::unique_ptr<StreamBuffer> stream_buffer)
        : rasterizer{rasterizer}, system{system}, stream_buffer{std::move(stream_buffer)},
          stream_buffer_handle{this->stream_buffer->GetHandle()} {}

    ~BufferCache() = default;

    virtual BufferType ToHandle(const OwnerBuffer& storage) = 0;

    virtual OwnerBuffer CreateBlock(VAddr cpu_addr, std::size_t size) = 0;

    virtual void UploadBlockData(const OwnerBuffer& buffer, std::size_t offset, std::size_t size,
                                 const u8* data) = 0;

    virtual void DownloadBlockData(const OwnerBuffer& buffer, std::size_t offset, std::size_t size,
                                   u8* data) = 0;

    virtual void CopyBlock(const OwnerBuffer& src, const OwnerBuffer& dst, std::size_t src_offset,
                           std::size_t dst_offset, std::size_t size) = 0;

    virtual BufferInfo ConstBufferUpload(const void* raw_pointer, std::size_t size) {
        return {};
    }

    /// Register an object into the cache
    void Register(const MapInterval& new_map, bool inherit_written = false) {
        const VAddr cpu_addr = new_map->GetStart();
        if (!cpu_addr) {
            LOG_CRITICAL(HW_GPU, "Failed to register buffer with unmapped gpu_address 0x{:016x}",
                         new_map->GetGpuAddress());
            return;
        }
        const std::size_t size = new_map->GetEnd() - new_map->GetStart();
        new_map->MarkAsRegistered(true);
        const IntervalType interval{new_map->GetStart(), new_map->GetEnd()};
        mapped_addresses.insert({interval, new_map});
        rasterizer.UpdatePagesCachedCount(cpu_addr, size, 1);
        new_map->SetMemoryMarked(true);
        if (inherit_written) {
            MarkRegionAsWritten(new_map->GetStart(), new_map->GetEnd() - 1);
            new_map->MarkAsWritten(true);
        }
    }

    void UnmarkMemory(const MapInterval& map) {
        if (!map->IsMemoryMarked()) {
            return;
        }
        const std::size_t size = map->GetEnd() - map->GetStart();
        rasterizer.UpdatePagesCachedCount(map->GetStart(), size, -1);
        map->SetMemoryMarked(false);
    }

    /// Unregisters an object from the cache
    void Unregister(const MapInterval& map) {
        UnmarkMemory(map);
        map->MarkAsRegistered(false);
        if (map->IsSyncPending()) {
            marked_for_unregister.remove(map);
            map->SetSyncPending(false);
        }
        if (map->IsWritten()) {
            UnmarkRegionAsWritten(map->GetStart(), map->GetEnd() - 1);
        }
        const IntervalType delete_interval{map->GetStart(), map->GetEnd()};
        mapped_addresses.erase(delete_interval);
    }

private:
    MapInterval CreateMap(const VAddr start, const VAddr end, const GPUVAddr gpu_addr) {
        return std::make_shared<MapIntervalBase>(start, end, gpu_addr);
    }

    MapInterval MapAddress(const OwnerBuffer& block, const GPUVAddr gpu_addr, const VAddr cpu_addr,
                           const std::size_t size) {
        std::vector<MapInterval> overlaps = GetMapsInRange(cpu_addr, size);
        if (overlaps.empty()) {
            auto& memory_manager = system.GPU().MemoryManager();
            const VAddr cpu_addr_end = cpu_addr + size;
            MapInterval new_map = CreateMap(cpu_addr, cpu_addr_end, gpu_addr);
            if (memory_manager.IsGranularRange(gpu_addr, size)) {
                u8* host_ptr = memory_manager.GetPointer(gpu_addr);
                UploadBlockData(block, block->GetOffset(cpu_addr), size, host_ptr);
            } else {
                staging_buffer.resize(size);
                memory_manager.ReadBlockUnsafe(gpu_addr, staging_buffer.data(), size);
                UploadBlockData(block, block->GetOffset(cpu_addr), size, staging_buffer.data());
            }
            Register(new_map);
            return new_map;
        }

        const VAddr cpu_addr_end = cpu_addr + size;
        if (overlaps.size() == 1) {
            MapInterval& current_map = overlaps[0];
            if (current_map->IsInside(cpu_addr, cpu_addr_end)) {
                return current_map;
            }
        }
        VAddr new_start = cpu_addr;
        VAddr new_end = cpu_addr_end;
        bool write_inheritance = false;
        bool modified_inheritance = false;
        // Calculate new buffer parameters
        for (auto& overlap : overlaps) {
            new_start = std::min(overlap->GetStart(), new_start);
            new_end = std::max(overlap->GetEnd(), new_end);
            write_inheritance |= overlap->IsWritten();
            modified_inheritance |= overlap->IsModified();
        }
        GPUVAddr new_gpu_addr = gpu_addr + new_start - cpu_addr;
        for (auto& overlap : overlaps) {
            Unregister(overlap);
        }
        UpdateBlock(block, new_start, new_end, overlaps);
        MapInterval new_map = CreateMap(new_start, new_end, new_gpu_addr);
        if (modified_inheritance) {
            new_map->MarkAsModified(true, GetModifiedTicks());
            if (Settings::IsGPULevelHigh() && Settings::values.use_asynchronous_gpu_emulation) {
                MarkForAsyncFlush(new_map);
            }
        }
        Register(new_map, write_inheritance);
        return new_map;
    }

    void UpdateBlock(const OwnerBuffer& block, VAddr start, VAddr end,
                     std::vector<MapInterval>& overlaps) {
        const IntervalType base_interval{start, end};
        IntervalSet interval_set{};
        interval_set.add(base_interval);
        for (auto& overlap : overlaps) {
            const IntervalType subtract{overlap->GetStart(), overlap->GetEnd()};
            interval_set.subtract(subtract);
        }
        for (auto& interval : interval_set) {
            std::size_t size = interval.upper() - interval.lower();
            if (size > 0) {
                staging_buffer.resize(size);
                system.Memory().ReadBlockUnsafe(interval.lower(), staging_buffer.data(), size);
                UploadBlockData(block, block->GetOffset(interval.lower()), size,
                                staging_buffer.data());
            }
        }
    }

    std::vector<MapInterval> GetMapsInRange(VAddr addr, std::size_t size) {
        if (size == 0) {
            return {};
        }

        std::vector<MapInterval> objects{};
        const IntervalType interval{addr, addr + size};
        for (auto& pair : boost::make_iterator_range(mapped_addresses.equal_range(interval))) {
            objects.push_back(pair.second);
        }

        return objects;
    }

    /// Returns a ticks counter used for tracking when cached objects were last modified
    u64 GetModifiedTicks() {
        return ++modified_ticks;
    }

    void FlushMap(MapInterval map) {
        std::size_t size = map->GetEnd() - map->GetStart();
        OwnerBuffer block = blocks[map->GetStart() >> block_page_bits];
        staging_buffer.resize(size);
        DownloadBlockData(block, block->GetOffset(map->GetStart()), size, staging_buffer.data());
        system.Memory().WriteBlockUnsafe(map->GetStart(), staging_buffer.data(), size);
        map->MarkAsModified(false, 0);
    }

    BufferInfo StreamBufferUpload(const void* raw_pointer, std::size_t size,
                                  std::size_t alignment) {
        AlignBuffer(alignment);
        const std::size_t uploaded_offset = buffer_offset;
        std::memcpy(buffer_ptr, raw_pointer, size);

        buffer_ptr += size;
        buffer_offset += size;
        return {stream_buffer_handle, uploaded_offset};
    }

    void AlignBuffer(std::size_t alignment) {
        // Align the offset, not the mapped pointer
        const std::size_t offset_aligned = Common::AlignUp(buffer_offset, alignment);
        buffer_ptr += offset_aligned - buffer_offset;
        buffer_offset = offset_aligned;
    }

    OwnerBuffer EnlargeBlock(OwnerBuffer buffer) {
        const std::size_t old_size = buffer->GetSize();
        const std::size_t new_size = old_size + block_page_size;
        const VAddr cpu_addr = buffer->GetCpuAddr();
        OwnerBuffer new_buffer = CreateBlock(cpu_addr, new_size);
        CopyBlock(buffer, new_buffer, 0, 0, old_size);
        buffer->SetEpoch(epoch);
        pending_destruction.push_back(buffer);
        const VAddr cpu_addr_end = cpu_addr + new_size - 1;
        u64 page_start = cpu_addr >> block_page_bits;
        const u64 page_end = cpu_addr_end >> block_page_bits;
        while (page_start <= page_end) {
            blocks[page_start] = new_buffer;
            ++page_start;
        }
        return new_buffer;
    }

    OwnerBuffer MergeBlocks(OwnerBuffer first, OwnerBuffer second) {
        const std::size_t size_1 = first->GetSize();
        const std::size_t size_2 = second->GetSize();
        const VAddr first_addr = first->GetCpuAddr();
        const VAddr second_addr = second->GetCpuAddr();
        const VAddr new_addr = std::min(first_addr, second_addr);
        const std::size_t new_size = size_1 + size_2;
        OwnerBuffer new_buffer = CreateBlock(new_addr, new_size);
        CopyBlock(first, new_buffer, 0, new_buffer->GetOffset(first_addr), size_1);
        CopyBlock(second, new_buffer, 0, new_buffer->GetOffset(second_addr), size_2);
        first->SetEpoch(epoch);
        second->SetEpoch(epoch);
        pending_destruction.push_back(first);
        pending_destruction.push_back(second);
        const VAddr cpu_addr_end = new_addr + new_size - 1;
        u64 page_start = new_addr >> block_page_bits;
        const u64 page_end = cpu_addr_end >> block_page_bits;
        while (page_start <= page_end) {
            blocks[page_start] = new_buffer;
            ++page_start;
        }
        return new_buffer;
    }

    OwnerBuffer GetBlock(const VAddr cpu_addr, const std::size_t size) {
        OwnerBuffer found;
        const VAddr cpu_addr_end = cpu_addr + size - 1;
        u64 page_start = cpu_addr >> block_page_bits;
        const u64 page_end = cpu_addr_end >> block_page_bits;
        while (page_start <= page_end) {
            auto it = blocks.find(page_start);
            if (it == blocks.end()) {
                if (found) {
                    found = EnlargeBlock(found);
                } else {
                    const VAddr start_addr = (page_start << block_page_bits);
                    found = CreateBlock(start_addr, block_page_size);
                    blocks[page_start] = found;
                }
            } else {
                if (found) {
                    if (found == it->second) {
                        ++page_start;
                        continue;
                    }
                    found = MergeBlocks(found, it->second);
                } else {
                    found = it->second;
                }
            }
            ++page_start;
        }
        return found;
    }

    void MarkRegionAsWritten(const VAddr start, const VAddr end) {
        u64 page_start = start >> write_page_bit;
        const u64 page_end = end >> write_page_bit;
        while (page_start <= page_end) {
            auto it = written_pages.find(page_start);
            if (it != written_pages.end()) {
                it->second = it->second + 1;
            } else {
                written_pages[page_start] = 1;
            }
            page_start++;
        }
    }

    void UnmarkRegionAsWritten(const VAddr start, const VAddr end) {
        u64 page_start = start >> write_page_bit;
        const u64 page_end = end >> write_page_bit;
        while (page_start <= page_end) {
            auto it = written_pages.find(page_start);
            if (it != written_pages.end()) {
                if (it->second > 1) {
                    it->second = it->second - 1;
                } else {
                    written_pages.erase(it);
                }
            }
            page_start++;
        }
    }

    bool IsRegionWritten(const VAddr start, const VAddr end) const {
        u64 page_start = start >> write_page_bit;
        const u64 page_end = end >> write_page_bit;
        while (page_start <= page_end) {
            if (written_pages.count(page_start) > 0) {
                return true;
            }
            page_start++;
        }
        return false;
    }

    void MarkForAsyncFlush(MapInterval& map) {
        if (!uncommitted_flushes) {
            uncommitted_flushes = std::make_shared<std::unordered_set<MapInterval>>();
        }
        uncommitted_flushes->insert(map);
    }

    VideoCore::RasterizerInterface& rasterizer;
    Core::System& system;

    std::unique_ptr<StreamBuffer> stream_buffer;
    BufferType stream_buffer_handle{};

    bool invalidated = false;

    u8* buffer_ptr = nullptr;
    u64 buffer_offset = 0;
    u64 buffer_offset_base = 0;

    using IntervalSet = boost::icl::interval_set<VAddr>;
    using IntervalCache = boost::icl::interval_map<VAddr, MapInterval>;
    using IntervalType = typename IntervalCache::interval_type;
    IntervalCache mapped_addresses;

    static constexpr u64 write_page_bit = 11;
    std::unordered_map<u64, u32> written_pages;

    static constexpr u64 block_page_bits = 21;
    static constexpr u64 block_page_size = 1ULL << block_page_bits;
    std::unordered_map<u64, OwnerBuffer> blocks;

    std::list<OwnerBuffer> pending_destruction;
    u64 epoch = 0;
    u64 modified_ticks = 0;

    std::vector<u8> staging_buffer;
    std::list<MapInterval> marked_for_unregister;

    std::shared_ptr<std::unordered_set<MapInterval>> uncommitted_flushes{};
    std::list<std::shared_ptr<std::list<MapInterval>>> committed_flushes;

    std::recursive_mutex mutex;
};

} // namespace VideoCommon
