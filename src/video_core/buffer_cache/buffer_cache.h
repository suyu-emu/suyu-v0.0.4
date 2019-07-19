// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "common/alignment.h"
#include "common/common_types.h"
#include "core/core.h"
#include "video_core/buffer_cache/map_interval.h"
#include "video_core/buffer_cache/buffer_block.h"
#include "video_core/memory_manager.h"

namespace VideoCore {
class RasterizerInterface;
}

namespace VideoCommon {

template <typename TBuffer, typename TBufferType, typename StreamBuffer>
class BufferCache {
public:
    using BufferInfo = std::pair<const TBufferType*, u64>;

    BufferInfo UploadMemory(GPUVAddr gpu_addr, std::size_t size, std::size_t alignment = 4,
                            bool is_written = false) {
        std::lock_guard lock{mutex};

        auto& memory_manager = system.GPU().MemoryManager();
        const auto host_ptr = memory_manager.GetPointer(gpu_addr);
        if (!host_ptr) {
            return {GetEmptyBuffer(size), 0};
        }
        const auto cache_addr = ToCacheAddr(host_ptr);

        auto block = GetBlock(cache_addr, size);
        MapAddress(block, gpu_addr, cache_addr, size, is_written);

        const u64 offset = static_cast<u64>(block->GetOffset(cache_addr));

        return {ToHandle(block), offset};
    }

    /// Uploads from a host memory. Returns the OpenGL buffer where it's located and its offset.
    BufferInfo UploadHostMemory(const void* raw_pointer, std::size_t size,
                                std::size_t alignment = 4) {
        std::lock_guard lock{mutex};
        return StreamBufferUpload(raw_pointer, size, alignment);
    }

    void Map(std::size_t max_size) {
        std::tie(buffer_ptr, buffer_offset_base, invalidated) = stream_buffer->Map(max_size, 4);
        buffer_offset = buffer_offset_base;
    }

    /// Finishes the upload stream, returns true on bindings invalidation.
    bool Unmap() {
        stream_buffer->Unmap(buffer_offset - buffer_offset_base);
        return std::exchange(invalidated, false);
    }

    void TickFrame() {
        ++epoch;
        while (!pending_destruction.empty()) {
            if (pending_destruction.front()->GetEpoch() + 1 > epoch) {
                break;
            }
            pending_destruction.pop_front();
        }
    }

    /// Write any cached resources overlapping the specified region back to memory
    void FlushRegion(CacheAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        // TODO
    }

    /// Mark the specified region as being invalidated
    void InvalidateRegion(CacheAddr addr, u64 size) {
        std::lock_guard lock{mutex};

        std::vector<MapInterval> objects = GetMapsInRange(addr, size);
        for (auto& object : objects) {
            Unregister(object);
        }
    }

    virtual const TBufferType* GetEmptyBuffer(std::size_t size) = 0;

protected:
    explicit BufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                         std::unique_ptr<StreamBuffer> stream_buffer)
        : rasterizer{rasterizer}, system{system}, stream_buffer{std::move(stream_buffer)},
          stream_buffer_handle{this->stream_buffer->GetHandle()} {}

    ~BufferCache() = default;

    virtual const TBufferType* ToHandle(const TBuffer& storage) = 0;

    virtual void WriteBarrier() = 0;

    virtual TBuffer CreateBlock(CacheAddr cache_addr, std::size_t size) = 0;

    virtual void UploadBlockData(const TBuffer& buffer, std::size_t offset, std::size_t size,
                                 const u8* data) = 0;

    virtual void DownloadBlockData(const TBuffer& buffer, std::size_t offset, std::size_t size,
                                   u8* data) = 0;

    virtual void CopyBlock(const TBuffer& src, const TBuffer& dst, std::size_t src_offset,
                           std::size_t dst_offset, std::size_t size) = 0;

    /// Register an object into the cache
    void Register(const MapInterval& new_interval, const GPUVAddr gpu_addr) {
        const CacheAddr cache_ptr = new_interval.start;
        const std::size_t size = new_interval.end - new_interval.start;
        const std::optional<VAddr> cpu_addr =
            system.GPU().MemoryManager().GpuToCpuAddress(gpu_addr);
        if (!cache_ptr || !cpu_addr) {
            LOG_CRITICAL(HW_GPU, "Failed to register buffer with unmapped gpu_address 0x{:016x}",
                         gpu_addr);
            return;
        }
        const IntervalType interval{new_interval.start, new_interval.end};
        mapped_addresses.insert(interval);
        map_storage[new_interval] = MapInfo{gpu_addr, *cpu_addr};

        rasterizer.UpdatePagesCachedCount(*cpu_addr, size, 1);
    }

    /// Unregisters an object from the cache
    void Unregister(const MapInterval& interval) {
        const MapInfo info = map_storage[interval];
        const std::size_t size = interval.end - interval.start;
        rasterizer.UpdatePagesCachedCount(info.cpu_addr, size, -1);
        const IntervalType delete_interval{interval.start, interval.end};
        mapped_addresses.erase(delete_interval);
        map_storage.erase(interval);
    }

private:
    void MapAddress(const TBuffer& block, const GPUVAddr gpu_addr, const CacheAddr cache_addr,
                    const std::size_t size, bool is_written) {

        std::vector<MapInterval> overlaps = GetMapsInRange(cache_addr, size);
        if (overlaps.empty()) {
            const CacheAddr cache_addr_end = cache_addr + size;
            MapInterval new_interval{cache_addr, cache_addr_end};
            if (!is_written) {
                u8* host_ptr = FromCacheAddr(cache_addr);
                UploadBlockData(block, block->GetOffset(cache_addr), size, host_ptr);
            }
            Register(new_interval, gpu_addr);
            return;
        }

        if (overlaps.size() == 1) {
            MapInterval current_map = overlaps[0];
            const CacheAddr cache_addr_end = cache_addr + size;
            if (current_map.IsInside(cache_addr, cache_addr_end)) {
                return;
            }
            const CacheAddr new_start = std::min(cache_addr, current_map.start);
            const CacheAddr new_end = std::max(cache_addr_end, current_map.end);
            const GPUVAddr new_gpu_addr = gpu_addr + new_start - cache_addr;
            const std::size_t new_size = static_cast<std::size_t>(new_end - new_start);
            MapInterval new_interval{new_start, new_end};
            const std::size_t offset = current_map.start - new_start;
            const std::size_t size = current_map.end - current_map.start;
            // Upload the remaining data
            if (!is_written) {
                u8* host_ptr = FromCacheAddr(new_start);
                if (new_start == cache_addr && new_end == cache_addr_end) {
                    std::size_t first_size = current_map.start - new_start;
                    if (first_size > 0) {
                        UploadBlockData(block, block->GetOffset(new_start), first_size, host_ptr);
                    }

                    std::size_t second_size = new_end - current_map.end;
                    if (second_size > 0) {
                        u8* host_ptr2 = FromCacheAddr(current_map.end);
                        UploadBlockData(block, block->GetOffset(current_map.end), second_size,
                                         host_ptr2);
                    }
                } else {
                    if (new_start == cache_addr) {
                        std::size_t second_size = new_end - current_map.end;
                        if (second_size > 0) {
                            u8* host_ptr2 = FromCacheAddr(current_map.end);
                            UploadBlockData(block, block->GetOffset(current_map.end), second_size,
                                             host_ptr2);
                        }
                    } else {
                        std::size_t first_size = current_map.start - new_start;
                        if (first_size > 0) {
                            UploadBlockData(block, block->GetOffset(new_start), first_size, host_ptr);
                        }
                    }
                }
            }
            Unregister(current_map);
            Register(new_interval, new_gpu_addr);
        } else {
            // Calculate new buffer parameters
            GPUVAddr new_gpu_addr = gpu_addr;
            CacheAddr start = cache_addr;
            CacheAddr end = cache_addr + size;
            for (auto& overlap : overlaps) {
                start = std::min(overlap.start, start);
                end = std::max(overlap.end, end);
            }
            new_gpu_addr = gpu_addr + start - cache_addr;
            MapInterval new_interval{start, end};
            for (auto& overlap : overlaps) {
                Unregister(overlap);
            }
            std::size_t new_size = end - start;
            if (!is_written) {
                u8* host_ptr = FromCacheAddr(start);
                UploadBlockData(block, block->GetOffset(start), new_size, host_ptr);
            }
            Register(new_interval, new_gpu_addr);
        }
    }

    std::vector<MapInterval> GetMapsInRange(CacheAddr addr, std::size_t size) {
        if (size == 0) {
            return {};
        }

        std::vector<MapInterval> objects{};
        const IntervalType interval{addr, addr + size};
        for (auto& pair : boost::make_iterator_range(mapped_addresses.equal_range(interval))) {
            objects.emplace_back(pair.lower(), pair.upper());
        }

        return objects;
    }

    /// Returns a ticks counter used for tracking when cached objects were last modified
    u64 GetModifiedTicks() {
        return ++modified_ticks;
    }

    BufferInfo StreamBufferUpload(const void* raw_pointer, std::size_t size,
                                  std::size_t alignment) {
        AlignBuffer(alignment);
        const std::size_t uploaded_offset = buffer_offset;
        std::memcpy(buffer_ptr, raw_pointer, size);

        buffer_ptr += size;
        buffer_offset += size;
        return {&stream_buffer_handle, uploaded_offset};
    }

    void AlignBuffer(std::size_t alignment) {
        // Align the offset, not the mapped pointer
        const std::size_t offset_aligned = Common::AlignUp(buffer_offset, alignment);
        buffer_ptr += offset_aligned - buffer_offset;
        buffer_offset = offset_aligned;
    }

    TBuffer EnlargeBlock(TBuffer buffer) {
        const std::size_t old_size = buffer->GetSize();
        const std::size_t new_size = old_size + block_page_size;
        const CacheAddr cache_addr = buffer->GetCacheAddr();
        TBuffer new_buffer = CreateBlock(cache_addr, new_size);
        CopyBlock(buffer, new_buffer, 0, 0, old_size);
        buffer->SetEpoch(epoch);
        pending_destruction.push_back(buffer);
        const CacheAddr cache_addr_end = cache_addr + new_size - 1;
        u64 page_start = cache_addr >> block_page_bits;
        const u64 page_end = cache_addr_end >> block_page_bits;
        while (page_start <= page_end) {
            blocks[page_start] = new_buffer;
            ++page_start;
        }
        return new_buffer;
    }

    TBuffer MergeBlocks(TBuffer first, TBuffer second) {
        const std::size_t size_1 = first->GetSize();
        const std::size_t size_2 = second->GetSize();
        const CacheAddr first_addr = first->GetCacheAddr();
        const CacheAddr second_addr = second->GetCacheAddr();
        const CacheAddr new_addr = std::min(first_addr, second_addr);
        const std::size_t new_size = size_1 + size_2;
        TBuffer new_buffer = CreateBlock(new_addr, new_size);
        CopyBlock(first, new_buffer, 0, new_buffer->GetOffset(first_addr), size_1);
        CopyBlock(second, new_buffer, 0, new_buffer->GetOffset(second_addr), size_2);
        first->SetEpoch(epoch);
        second->SetEpoch(epoch);
        pending_destruction.push_back(first);
        pending_destruction.push_back(second);
        const CacheAddr cache_addr_end = new_addr + new_size - 1;
        u64 page_start = new_addr >> block_page_bits;
        const u64 page_end = cache_addr_end >> block_page_bits;
        while (page_start <= page_end) {
            blocks[page_start] = new_buffer;
            ++page_start;
        }
        return new_buffer;
    }

    TBuffer GetBlock(const CacheAddr cache_addr, const std::size_t size) {
        TBuffer found{};
        const CacheAddr cache_addr_end = cache_addr + size - 1;
        u64 page_start = cache_addr >> block_page_bits;
        const u64 page_end = cache_addr_end >> block_page_bits;
        const u64 num_pages = page_end - page_start + 1;
        while (page_start <= page_end) {
            auto it = blocks.find(page_start);
            if (it == blocks.end()) {
                if (found) {
                    found = EnlargeBlock(found);
                } else {
                    const CacheAddr start_addr = (page_start << block_page_bits);
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

    std::unique_ptr<StreamBuffer> stream_buffer;
    TBufferType stream_buffer_handle{};

    bool invalidated = false;

    u8* buffer_ptr = nullptr;
    u64 buffer_offset = 0;
    u64 buffer_offset_base = 0;

    using IntervalCache = boost::icl::interval_set<CacheAddr>;
    using IntervalType = typename IntervalCache::interval_type;
    IntervalCache mapped_addresses{};
    std::unordered_map<MapInterval, MapInfo> map_storage;

    static constexpr u64 block_page_bits{24};
    static constexpr u64 block_page_size{1 << block_page_bits};
    std::unordered_map<u64, TBuffer> blocks;

    std::list<TBuffer> pending_destruction;
    u64 epoch{};
    u64 modified_ticks{};
    VideoCore::RasterizerInterface& rasterizer;
    Core::System& system;
    std::recursive_mutex mutex;
};

} // namespace VideoCommon
