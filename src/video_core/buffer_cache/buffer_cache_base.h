// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <numeric>
#include <span>
#include <unordered_map>
#include <vector>

#include <boost/container/small_vector.hpp>
#define BOOST_NO_MT
#include <boost/pool/detail/mutex.hpp>
#undef BOOST_NO_MT
#include <boost/icl/interval.hpp>
#include <boost/icl/interval_base_set.hpp>
#include <boost/icl/interval_set.hpp>
#include <boost/icl/split_interval_map.hpp>
#include <boost/pool/pool.hpp>
#include <boost/pool/pool_alloc.hpp>
#include <boost/pool/poolfwd.hpp>

#include "common/common_types.h"
#include "common/div_ceil.h"
#include "common/literals.h"
#include "common/lru_cache.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/settings.h"
#include "core/memory.h"
#include "video_core/buffer_cache/buffer_base.h"
#include "video_core/control/channel_state_cache.h"
#include "video_core/delayed_destruction_ring.h"
#include "video_core/dirty_flags.h"
#include "video_core/engines/draw_manager.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/slot_vector.h"
#include "video_core/texture_cache/types.h"

namespace boost {
template <typename T>
class fast_pool_allocator<T, default_user_allocator_new_delete, details::pool::null_mutex, 4096, 0>;
}

namespace VideoCommon {

MICROPROFILE_DECLARE(GPU_PrepareBuffers);
MICROPROFILE_DECLARE(GPU_BindUploadBuffers);
MICROPROFILE_DECLARE(GPU_DownloadMemory);

using BufferId = SlotId;

using VideoCore::Surface::PixelFormat;
using namespace Common::Literals;

constexpr u32 NUM_VERTEX_BUFFERS = 32;
constexpr u32 NUM_TRANSFORM_FEEDBACK_BUFFERS = 4;
constexpr u32 NUM_GRAPHICS_UNIFORM_BUFFERS = 18;
constexpr u32 NUM_COMPUTE_UNIFORM_BUFFERS = 8;
constexpr u32 NUM_STORAGE_BUFFERS = 16;
constexpr u32 NUM_TEXTURE_BUFFERS = 32;
constexpr u32 NUM_STAGES = 5;

using UniformBufferSizes = std::array<std::array<u32, NUM_GRAPHICS_UNIFORM_BUFFERS>, NUM_STAGES>;
using ComputeUniformBufferSizes = std::array<u32, NUM_COMPUTE_UNIFORM_BUFFERS>;

enum class ObtainBufferSynchronize : u32 {
    NoSynchronize = 0,
    FullSynchronize = 1,
    SynchronizeNoDirty = 2,
};

enum class ObtainBufferOperation : u32 {
    DoNothing = 0,
    MarkAsWritten = 1,
    DiscardWrite = 2,
    MarkQuery = 3,
};

static constexpr BufferId NULL_BUFFER_ID{0};
static constexpr u32 DEFAULT_SKIP_CACHE_SIZE = static_cast<u32>(4_KiB);

struct Binding {
    VAddr cpu_addr{};
    u32 size{};
    BufferId buffer_id;
};

struct TextureBufferBinding : Binding {
    PixelFormat format;
};

static constexpr Binding NULL_BINDING{
    .cpu_addr = 0,
    .size = 0,
    .buffer_id = NULL_BUFFER_ID,
};

template <typename Buffer>
struct HostBindings {
    boost::container::small_vector<Buffer*, NUM_VERTEX_BUFFERS> buffers;
    boost::container::small_vector<u64, NUM_VERTEX_BUFFERS> offsets;
    boost::container::small_vector<u64, NUM_VERTEX_BUFFERS> sizes;
    boost::container::small_vector<u64, NUM_VERTEX_BUFFERS> strides;
    u32 min_index{NUM_VERTEX_BUFFERS};
    u32 max_index{0};
};

class BufferCacheChannelInfo : public ChannelInfo {
public:
    BufferCacheChannelInfo() = delete;
    BufferCacheChannelInfo(Tegra::Control::ChannelState& state) noexcept : ChannelInfo(state) {}
    BufferCacheChannelInfo(const BufferCacheChannelInfo& state) = delete;
    BufferCacheChannelInfo& operator=(const BufferCacheChannelInfo&) = delete;

    Binding index_buffer;
    std::array<Binding, NUM_VERTEX_BUFFERS> vertex_buffers;
    std::array<std::array<Binding, NUM_GRAPHICS_UNIFORM_BUFFERS>, NUM_STAGES> uniform_buffers;
    std::array<std::array<Binding, NUM_STORAGE_BUFFERS>, NUM_STAGES> storage_buffers;
    std::array<std::array<TextureBufferBinding, NUM_TEXTURE_BUFFERS>, NUM_STAGES> texture_buffers;
    std::array<Binding, NUM_TRANSFORM_FEEDBACK_BUFFERS> transform_feedback_buffers;
    Binding count_buffer_binding;
    Binding indirect_buffer_binding;

    std::array<Binding, NUM_COMPUTE_UNIFORM_BUFFERS> compute_uniform_buffers;
    std::array<Binding, NUM_STORAGE_BUFFERS> compute_storage_buffers;
    std::array<TextureBufferBinding, NUM_TEXTURE_BUFFERS> compute_texture_buffers;

    std::array<u32, NUM_STAGES> enabled_uniform_buffer_masks{};
    u32 enabled_compute_uniform_buffer_mask = 0;

    const UniformBufferSizes* uniform_buffer_sizes{};
    const ComputeUniformBufferSizes* compute_uniform_buffer_sizes{};

    std::array<u32, NUM_STAGES> enabled_storage_buffers{};
    std::array<u32, NUM_STAGES> written_storage_buffers{};
    u32 enabled_compute_storage_buffers = 0;
    u32 written_compute_storage_buffers = 0;

    std::array<u32, NUM_STAGES> enabled_texture_buffers{};
    std::array<u32, NUM_STAGES> written_texture_buffers{};
    std::array<u32, NUM_STAGES> image_texture_buffers{};
    u32 enabled_compute_texture_buffers = 0;
    u32 written_compute_texture_buffers = 0;
    u32 image_compute_texture_buffers = 0;

    std::array<u32, 16> uniform_cache_hits{};
    std::array<u32, 16> uniform_cache_shots{};

    u32 uniform_buffer_skip_cache_size = DEFAULT_SKIP_CACHE_SIZE;

    bool has_deleted_buffers = false;

    std::array<u32, NUM_STAGES> dirty_uniform_buffers{};
    std::array<u32, NUM_STAGES> fast_bound_uniform_buffers{};
    std::array<std::array<u32, NUM_GRAPHICS_UNIFORM_BUFFERS>, NUM_STAGES>
        uniform_buffer_binding_sizes{};
};

template <class P>
class BufferCache : public VideoCommon::ChannelSetupCaches<BufferCacheChannelInfo> {
    // Page size for caching purposes.
    // This is unrelated to the CPU page size and it can be changed as it seems optimal.
    static constexpr u32 CACHING_PAGEBITS = 16;
    static constexpr u64 CACHING_PAGESIZE = u64{1} << CACHING_PAGEBITS;

    static constexpr bool IS_OPENGL = P::IS_OPENGL;
    static constexpr bool HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS =
        P::HAS_PERSISTENT_UNIFORM_BUFFER_BINDINGS;
    static constexpr bool HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT =
        P::HAS_FULL_INDEX_AND_PRIMITIVE_SUPPORT;
    static constexpr bool NEEDS_BIND_UNIFORM_INDEX = P::NEEDS_BIND_UNIFORM_INDEX;
    static constexpr bool NEEDS_BIND_STORAGE_INDEX = P::NEEDS_BIND_STORAGE_INDEX;
    static constexpr bool USE_MEMORY_MAPS = P::USE_MEMORY_MAPS;
    static constexpr bool SEPARATE_IMAGE_BUFFERS_BINDINGS = P::SEPARATE_IMAGE_BUFFER_BINDINGS;
    static constexpr bool IMPLEMENTS_ASYNC_DOWNLOADS = P::IMPLEMENTS_ASYNC_DOWNLOADS;
    static constexpr bool USE_MEMORY_MAPS_FOR_UPLOADS = P::USE_MEMORY_MAPS_FOR_UPLOADS;

    static constexpr s64 DEFAULT_EXPECTED_MEMORY = 512_MiB;
    static constexpr s64 DEFAULT_CRITICAL_MEMORY = 1_GiB;
    static constexpr s64 TARGET_THRESHOLD = 4_GiB;

    // Debug Flags.

    static constexpr bool DISABLE_DOWNLOADS = true;

    using Maxwell = Tegra::Engines::Maxwell3D::Regs;

    using Runtime = typename P::Runtime;
    using Buffer = typename P::Buffer;
    using Async_Buffer = typename P::Async_Buffer;
    using MemoryTracker = typename P::MemoryTracker;

    using IntervalCompare = std::less<VAddr>;
    using IntervalInstance = boost::icl::interval_type_default<VAddr, std::less>;
    using IntervalAllocator = boost::fast_pool_allocator<VAddr>;
    using IntervalSet = boost::icl::interval_set<VAddr>;
    using IntervalType = typename IntervalSet::interval_type;

    template <typename Type>
    struct counter_add_functor : public boost::icl::identity_based_inplace_combine<Type> {
        // types
        typedef counter_add_functor<Type> type;
        typedef boost::icl::identity_based_inplace_combine<Type> base_type;

        // public member functions
        void operator()(Type& current, const Type& added) const {
            current += added;
            if (current < base_type::identity_element()) {
                current = base_type::identity_element();
            }
        }

        // public static functions
        static void version(Type&){};
    };

    using OverlapCombine = counter_add_functor<int>;
    using OverlapSection = boost::icl::inter_section<int>;
    using OverlapCounter = boost::icl::split_interval_map<VAddr, int>;

    struct OverlapResult {
        boost::container::small_vector<BufferId, 16> ids;
        VAddr begin;
        VAddr end;
        bool has_stream_leap = false;
    };

public:
    explicit BufferCache(VideoCore::RasterizerInterface& rasterizer_,
                         Core::Memory::Memory& cpu_memory_, Runtime& runtime_);

    void TickFrame();

    void WriteMemory(VAddr cpu_addr, u64 size);

    void CachedWriteMemory(VAddr cpu_addr, u64 size);

    bool OnCPUWrite(VAddr cpu_addr, u64 size);

    void DownloadMemory(VAddr cpu_addr, u64 size);

    std::optional<VideoCore::RasterizerDownloadArea> GetFlushArea(VAddr cpu_addr, u64 size);

    bool InlineMemory(VAddr dest_address, size_t copy_size, std::span<const u8> inlined_buffer);

    void BindGraphicsUniformBuffer(size_t stage, u32 index, GPUVAddr gpu_addr, u32 size);

    void DisableGraphicsUniformBuffer(size_t stage, u32 index);

    void UpdateGraphicsBuffers(bool is_indexed);

    void UpdateComputeBuffers();

    void BindHostGeometryBuffers(bool is_indexed);

    void BindHostStageBuffers(size_t stage);

    void BindHostComputeBuffers();

    void SetUniformBuffersState(const std::array<u32, NUM_STAGES>& mask,
                                const UniformBufferSizes* sizes);

    void SetComputeUniformBufferState(u32 mask, const ComputeUniformBufferSizes* sizes);

    void UnbindGraphicsStorageBuffers(size_t stage);

    void BindGraphicsStorageBuffer(size_t stage, size_t ssbo_index, u32 cbuf_index, u32 cbuf_offset,
                                   bool is_written);

    void UnbindGraphicsTextureBuffers(size_t stage);

    void BindGraphicsTextureBuffer(size_t stage, size_t tbo_index, GPUVAddr gpu_addr, u32 size,
                                   PixelFormat format, bool is_written, bool is_image);

    void UnbindComputeStorageBuffers();

    void BindComputeStorageBuffer(size_t ssbo_index, u32 cbuf_index, u32 cbuf_offset,
                                  bool is_written);

    void UnbindComputeTextureBuffers();

    void BindComputeTextureBuffer(size_t tbo_index, GPUVAddr gpu_addr, u32 size, PixelFormat format,
                                  bool is_written, bool is_image);

    [[nodiscard]] std::pair<Buffer*, u32> ObtainBuffer(GPUVAddr gpu_addr, u32 size,
                                                       ObtainBufferSynchronize sync_info,
                                                       ObtainBufferOperation post_op);

    [[nodiscard]] std::pair<Buffer*, u32> ObtainCPUBuffer(VAddr gpu_addr, u32 size,
                                                          ObtainBufferSynchronize sync_info,
                                                          ObtainBufferOperation post_op);
    void FlushCachedWrites();

    /// Return true when there are uncommitted buffers to be downloaded
    [[nodiscard]] bool HasUncommittedFlushes() const noexcept;

    void AccumulateFlushes();

    /// Return true when the caller should wait for async downloads
    [[nodiscard]] bool ShouldWaitAsyncFlushes() const noexcept;

    /// Commit asynchronous downloads
    void CommitAsyncFlushes();
    void CommitAsyncFlushesHigh();

    /// Pop asynchronous downloads
    void PopAsyncFlushes();
    void PopAsyncBuffers();

    bool DMACopy(GPUVAddr src_address, GPUVAddr dest_address, u64 amount);

    bool DMAClear(GPUVAddr src_address, u64 amount, u32 value);

    /// Return true when a CPU region is modified from the GPU
    [[nodiscard]] bool IsRegionGpuModified(VAddr addr, size_t size);

    /// Return true when a region is registered on the cache
    [[nodiscard]] bool IsRegionRegistered(VAddr addr, size_t size);

    /// Return true when a CPU region is modified from the CPU
    [[nodiscard]] bool IsRegionCpuModified(VAddr addr, size_t size);

    void SetDrawIndirect(
        const Tegra::Engines::DrawManager::IndirectParams* current_draw_indirect_) {
        current_draw_indirect = current_draw_indirect_;
    }

    [[nodiscard]] std::pair<Buffer*, u32> GetDrawIndirectCount();

    [[nodiscard]] std::pair<Buffer*, u32> GetDrawIndirectBuffer();

    template <typename Func>
    void BufferOperations(Func&& func) {
        do {
            channel_state->has_deleted_buffers = false;
            func();
        } while (channel_state->has_deleted_buffers);
    }

    std::recursive_mutex mutex;
    Runtime& runtime;

private:
    template <typename Func>
    static void ForEachEnabledBit(u32 enabled_mask, Func&& func) {
        for (u32 index = 0; enabled_mask != 0; ++index, enabled_mask >>= 1) {
            const int disabled_bits = std::countr_zero(enabled_mask);
            index += disabled_bits;
            enabled_mask >>= disabled_bits;
            func(index);
        }
    }

    template <typename Func>
    void ForEachBufferInRange(VAddr cpu_addr, u64 size, Func&& func) {
        const u64 page_end = Common::DivCeil(cpu_addr + size, CACHING_PAGESIZE);
        for (u64 page = cpu_addr >> CACHING_PAGEBITS; page < page_end;) {
            const BufferId buffer_id = page_table[page];
            if (!buffer_id) {
                ++page;
                continue;
            }
            Buffer& buffer = slot_buffers[buffer_id];
            func(buffer_id, buffer);

            const VAddr end_addr = buffer.CpuAddr() + buffer.SizeBytes();
            page = Common::DivCeil(end_addr, CACHING_PAGESIZE);
        }
    }

    template <typename Func>
    void ForEachInRangeSet(IntervalSet& current_range, VAddr cpu_addr, u64 size, Func&& func) {
        const VAddr start_address = cpu_addr;
        const VAddr end_address = start_address + size;
        const IntervalType search_interval{start_address, end_address};
        auto it = current_range.lower_bound(search_interval);
        if (it == current_range.end()) {
            return;
        }
        auto end_it = current_range.upper_bound(search_interval);
        for (; it != end_it; it++) {
            VAddr inter_addr_end = it->upper();
            VAddr inter_addr = it->lower();
            if (inter_addr_end > end_address) {
                inter_addr_end = end_address;
            }
            if (inter_addr < start_address) {
                inter_addr = start_address;
            }
            func(inter_addr, inter_addr_end);
        }
    }

    template <typename Func>
    void ForEachInOverlapCounter(OverlapCounter& current_range, VAddr cpu_addr, u64 size,
                                 Func&& func) {
        const VAddr start_address = cpu_addr;
        const VAddr end_address = start_address + size;
        const IntervalType search_interval{start_address, end_address};
        auto it = current_range.lower_bound(search_interval);
        if (it == current_range.end()) {
            return;
        }
        auto end_it = current_range.upper_bound(search_interval);
        for (; it != end_it; it++) {
            auto& inter = it->first;
            VAddr inter_addr_end = inter.upper();
            VAddr inter_addr = inter.lower();
            if (inter_addr_end > end_address) {
                inter_addr_end = end_address;
            }
            if (inter_addr < start_address) {
                inter_addr = start_address;
            }
            func(inter_addr, inter_addr_end, it->second);
        }
    }

    void RemoveEachInOverlapCounter(OverlapCounter& current_range,
                                    const IntervalType search_interval, int subtract_value) {
        bool any_removals = false;
        current_range.add(std::make_pair(search_interval, subtract_value));
        do {
            any_removals = false;
            auto it = current_range.lower_bound(search_interval);
            if (it == current_range.end()) {
                return;
            }
            auto end_it = current_range.upper_bound(search_interval);
            for (; it != end_it; it++) {
                if (it->second <= 0) {
                    any_removals = true;
                    current_range.erase(it);
                    break;
                }
            }
        } while (any_removals);
    }

    static bool IsRangeGranular(VAddr cpu_addr, size_t size) {
        return (cpu_addr & ~Core::Memory::YUZU_PAGEMASK) ==
               ((cpu_addr + size) & ~Core::Memory::YUZU_PAGEMASK);
    }

    void RunGarbageCollector();

    void BindHostIndexBuffer();

    void BindHostVertexBuffers();

    void BindHostDrawIndirectBuffers();

    void BindHostGraphicsUniformBuffers(size_t stage);

    void BindHostGraphicsUniformBuffer(size_t stage, u32 index, u32 binding_index, bool needs_bind);

    void BindHostGraphicsStorageBuffers(size_t stage);

    void BindHostGraphicsTextureBuffers(size_t stage);

    void BindHostTransformFeedbackBuffers();

    void BindHostComputeUniformBuffers();

    void BindHostComputeStorageBuffers();

    void BindHostComputeTextureBuffers();

    void DoUpdateGraphicsBuffers(bool is_indexed);

    void DoUpdateComputeBuffers();

    void UpdateIndexBuffer();

    void UpdateVertexBuffers();

    void UpdateVertexBuffer(u32 index);

    void UpdateDrawIndirect();

    void UpdateUniformBuffers(size_t stage);

    void UpdateStorageBuffers(size_t stage);

    void UpdateTextureBuffers(size_t stage);

    void UpdateTransformFeedbackBuffers();

    void UpdateTransformFeedbackBuffer(u32 index);

    void UpdateComputeUniformBuffers();

    void UpdateComputeStorageBuffers();

    void UpdateComputeTextureBuffers();

    void MarkWrittenBuffer(BufferId buffer_id, VAddr cpu_addr, u32 size);

    [[nodiscard]] BufferId FindBuffer(VAddr cpu_addr, u32 size);

    [[nodiscard]] OverlapResult ResolveOverlaps(VAddr cpu_addr, u32 wanted_size);

    void JoinOverlap(BufferId new_buffer_id, BufferId overlap_id, bool accumulate_stream_score);

    [[nodiscard]] BufferId CreateBuffer(VAddr cpu_addr, u32 wanted_size);

    void Register(BufferId buffer_id);

    void Unregister(BufferId buffer_id);

    template <bool insert>
    void ChangeRegister(BufferId buffer_id);

    void TouchBuffer(Buffer& buffer, BufferId buffer_id) noexcept;

    bool SynchronizeBuffer(Buffer& buffer, VAddr cpu_addr, u32 size);

    bool SynchronizeBufferImpl(Buffer& buffer, VAddr cpu_addr, u32 size);

    bool SynchronizeBufferNoModified(Buffer& buffer, VAddr cpu_addr, u32 size);

    void UploadMemory(Buffer& buffer, u64 total_size_bytes, u64 largest_copy,
                      std::span<BufferCopy> copies);

    void ImmediateUploadMemory(Buffer& buffer, u64 largest_copy,
                               std::span<const BufferCopy> copies);

    void MappedUploadMemory(Buffer& buffer, u64 total_size_bytes, std::span<BufferCopy> copies);

    void DownloadBufferMemory(Buffer& buffer_id);

    void DownloadBufferMemory(Buffer& buffer_id, VAddr cpu_addr, u64 size);

    void DeleteBuffer(BufferId buffer_id, bool do_not_mark = false);

    [[nodiscard]] Binding StorageBufferBinding(GPUVAddr ssbo_addr, u32 cbuf_index,
                                               bool is_written) const;

    [[nodiscard]] TextureBufferBinding GetTextureBufferBinding(GPUVAddr gpu_addr, u32 size,
                                                               PixelFormat format);

    [[nodiscard]] std::span<const u8> ImmediateBufferWithData(VAddr cpu_addr, size_t size);

    [[nodiscard]] std::span<u8> ImmediateBuffer(size_t wanted_capacity);

    [[nodiscard]] bool HasFastUniformBufferBound(size_t stage, u32 binding_index) const noexcept;

    void ClearDownload(IntervalType subtract_interval);

    void InlineMemoryImplementation(VAddr dest_address, size_t copy_size,
                                    std::span<const u8> inlined_buffer);

    VideoCore::RasterizerInterface& rasterizer;
    Core::Memory::Memory& cpu_memory;

    SlotVector<Buffer> slot_buffers;
    DelayedDestructionRing<Buffer, 8> delayed_destruction_ring;

    const Tegra::Engines::DrawManager::IndirectParams* current_draw_indirect{};

    u32 last_index_count = 0;

    MemoryTracker memory_tracker;
    IntervalSet uncommitted_ranges;
    IntervalSet common_ranges;
    IntervalSet cached_ranges;
    std::deque<IntervalSet> committed_ranges;

    // Async Buffers
    OverlapCounter async_downloads;
    std::deque<std::optional<Async_Buffer>> async_buffers;
    std::deque<boost::container::small_vector<BufferCopy, 4>> pending_downloads;
    std::optional<Async_Buffer> current_buffer;

    std::deque<Async_Buffer> async_buffers_death_ring;

    size_t immediate_buffer_capacity = 0;
    Common::ScratchBuffer<u8> immediate_buffer_alloc;

    struct LRUItemParams {
        using ObjectType = BufferId;
        using TickType = u64;
    };
    Common::LeastRecentlyUsedCache<LRUItemParams> lru_cache;
    u64 frame_tick = 0;
    u64 total_used_memory = 0;
    u64 minimum_memory = 0;
    u64 critical_memory = 0;
    BufferId inline_buffer_id;

    std::array<BufferId, ((1ULL << 39) >> CACHING_PAGEBITS)> page_table;
    Common::ScratchBuffer<u8> tmp_buffer;
};

} // namespace VideoCommon
