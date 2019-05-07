// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <unordered_map>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/gpu.h"
#include "video_core/morton.h"
#include "video_core/texture_cache/copy_params.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/texture_cache/surface_view.h"

template <class ForwardIt, class T, class Compare = std::less<>>
ForwardIt binary_find(ForwardIt first, ForwardIt last, const T& value, Compare comp = {}) {
    // Note: BOTH type T and the type after ForwardIt is dereferenced
    // must be implicitly convertible to BOTH Type1 and Type2, used in Compare.
    // This is stricter than lower_bound requirement (see above)

    first = std::lower_bound(first, last, value, comp);
    return first != last && !comp(value, *first) ? first : last;
}

namespace Tegra {
class MemoryManager;
}

namespace VideoCommon {

using VideoCore::MortonSwizzleMode;
using VideoCore::Surface::SurfaceTarget;

enum class MatchStructureResult : u32 {
    FullMatch = 0,
    SemiMatch = 1,
    None = 2,
};

class SurfaceBaseImpl {
public:
    void LoadBuffer(Tegra::MemoryManager& memory_manager, std::vector<u8>& staging_buffer);

    void FlushBuffer(std::vector<u8>& staging_buffer);

    GPUVAddr GetGpuAddr() const {
        return gpu_addr;
    }

    GPUVAddr GetGpuAddrEnd() const {
        return gpu_addr_end;
    }

    bool Overlaps(const GPUVAddr start, const GPUVAddr end) const {
        return (gpu_addr < end) && (gpu_addr_end > start);
    }

    // Use only when recycling a surface
    void SetGpuAddr(const GPUVAddr new_addr) {
        gpu_addr = new_addr;
        gpu_addr_end = new_addr + memory_size;
    }

    VAddr GetCpuAddr() const {
        return gpu_addr;
    }

    void SetCpuAddr(const VAddr new_addr) {
        cpu_addr = new_addr;
    }

    u8* GetHostPtr() const {
        return host_ptr;
    }

    void SetHostPtr(u8* new_addr) {
        host_ptr = new_addr;
    }

    const SurfaceParams& GetSurfaceParams() const {
        return params;
    }

    std::size_t GetSizeInBytes() const {
        return memory_size;
    }

    std::size_t GetHostSizeInBytes() const {
        return host_memory_size;
    }

    std::size_t GetMipmapSize(const u32 level) const {
        return mipmap_sizes[level];
    }

    bool MatchFormat(VideoCore::Surface::PixelFormat pixel_format) const {
        return params.pixel_format == pixel_format;
    }

    bool MatchTarget(VideoCore::Surface::SurfaceTarget target) const {
        return params.target == target;
    }

    bool MatchesTopology(const SurfaceParams& rhs) const {
        const u32 src_bpp = params.GetBytesPerPixel();
        const u32 dst_bpp = rhs.GetBytesPerPixel();
        return std::tie(src_bpp, params.is_tiled) == std::tie(dst_bpp, rhs.is_tiled);
    }

    MatchStructureResult MatchesStructure(const SurfaceParams& rhs) const {
        if (params.is_tiled) {
            if (std::tie(params.height, params.depth, params.block_width, params.block_height,
                         params.block_depth, params.tile_width_spacing) ==
                std::tie(rhs.height, rhs.depth, rhs.block_width, rhs.block_height, rhs.block_depth,
                         rhs.tile_width_spacing)) {
                if (params.width == rhs.width) {
                    return MatchStructureResult::FullMatch;
                }
                if (params.GetBlockAlignedWidth() == rhs.GetBlockAlignedWidth()) {
                    return MatchStructureResult::SemiMatch;
                }
            }
            return MatchStructureResult::None;
        } else {
            if (std::tie(params.width, params.height, params.pitch) ==
                std::tie(rhs.width, rhs.height, rhs.pitch)) {
                return MatchStructureResult::FullMatch;
            }
            return MatchStructureResult::None;
        }
    }

    std::optional<std::pair<u32, u32>> GetLayerMipmap(const GPUVAddr candidate_gpu_addr) const {
        if (candidate_gpu_addr < gpu_addr)
            return {};
        const GPUVAddr relative_address = candidate_gpu_addr - gpu_addr;
        const u32 layer = relative_address / layer_size;
        const GPUVAddr mipmap_address = relative_address - layer_size * layer;
        const auto mipmap_it =
            binary_find(mipmap_offsets.begin(), mipmap_offsets.end(), mipmap_address);
        if (mipmap_it != mipmap_offsets.end()) {
            return {{layer, std::distance(mipmap_offsets.begin(), mipmap_it)}};
        }
        return {};
    }

    std::vector<CopyParams> BreakDown(const SurfaceParams& in_params) const {
        auto set_up_copy = [](CopyParams& cp, const u32 width, const u32 height, const u32 depth,
                              const u32 level) {
            cp.source_x = 0;
            cp.source_y = 0;
            cp.source_z = 0;
            cp.dest_x = 0;
            cp.dest_y = 0;
            cp.dest_z = 0;
            cp.source_level = level;
            cp.dest_level = level;
            cp.width = width;
            cp.height = height;
            cp.depth = depth;
        };
        const u32 layers = params.depth;
        const u32 mipmaps = params.num_levels;
        if (params.is_layered) {
            std::vector<CopyParams> result{layers * mipmaps};
            for (std::size_t layer = 0; layer < layers; layer++) {
                const u32 layer_offset = layer * mipmaps;
                for (std::size_t level = 0; level < mipmaps; level++) {
                    CopyParams& cp = result[layer_offset + level];
                    const u32 width =
                        std::min(params.GetMipWidth(level), in_params.GetMipWidth(level));
                    const u32 height =
                        std::min(params.GetMipHeight(level), in_params.GetMipHeight(level));
                    set_up_copy(cp, width, height, layer, level);
                }
            }
            return result;
        } else {
            std::vector<CopyParams> result{mipmaps};
            for (std::size_t level = 0; level < mipmaps; level++) {
                CopyParams& cp = result[level];
                const u32 width = std::min(params.GetMipWidth(level), in_params.GetMipWidth(level));
                const u32 height =
                    std::min(params.GetMipHeight(level), in_params.GetMipHeight(level));
                const u32 depth = std::min(params.GetMipDepth(level), in_params.GetMipDepth(level));
                set_up_copy(cp, width, height, depth, level);
            }
            return result;
        }
    }

protected:
    explicit SurfaceBaseImpl(const GPUVAddr gpu_vaddr, const SurfaceParams& params);
    ~SurfaceBaseImpl() = default;

    virtual void DecorateSurfaceName() = 0;

    const SurfaceParams params;
    GPUVAddr gpu_addr{};
    GPUVAddr gpu_addr_end{};
    std::vector<u32> mipmap_sizes;
    std::vector<u32> mipmap_offsets;
    const std::size_t layer_size;
    const std::size_t memory_size;
    const std::size_t host_memory_size;
    u8* host_ptr;
    VAddr cpu_addr;

private:
    void SwizzleFunc(MortonSwizzleMode mode, u8* memory, const SurfaceParams& params, u8* buffer,
                     u32 level);
};

template <typename TView>
class SurfaceBase : public SurfaceBaseImpl {
public:
    virtual void UploadTexture(std::vector<u8>& staging_buffer) = 0;

    virtual void DownloadTexture(std::vector<u8>& staging_buffer) = 0;

    void MarkAsModified(const bool is_modified_, const u64 tick) {
        is_modified = is_modified_ || is_protected;
        modification_tick = tick;
    }

    void MarkAsProtected(const bool is_protected) {
        this->is_protected = is_protected;
    }

    void MarkAsPicked(const bool is_picked) {
        this->is_picked = is_picked;
    }

    bool IsModified() const {
        return is_modified;
    }

    bool IsProtected() const {
        return is_protected;
    }

    bool IsRegistered() const {
        return is_registered;
    }

    bool IsPicked() const {
        return is_picked;
    }

    void MarkAsRegistered(bool is_reg) {
        is_registered = is_reg;
    }

    u64 GetModificationTick() const {
        return modification_tick;
    }

    TView EmplaceOverview(const SurfaceParams& overview_params) {
        ViewParams vp{};
        vp.base_level = 0;
        vp.num_levels = params.num_levels;
        vp.target = overview_params.target;
        if (params.is_layered && !overview_params.is_layered) {
            vp.base_layer = 0;
            vp.num_layers = 1;
        } else {
            vp.base_layer = 0;
            vp.num_layers = params.depth;
        }
        return GetView(vp);
    }

    std::optional<TView> EmplaceView(const SurfaceParams& view_params, const GPUVAddr view_addr) {
        if (view_addr < gpu_addr)
            return {};
        if (params.target == SurfaceTarget::Texture3D ||
            view_params.target == SurfaceTarget::Texture3D) {
            return {};
        }
        const std::size_t size = view_params.GetGuestSizeInBytes();
        const GPUVAddr relative_address = view_addr - gpu_addr;
        auto layer_mipmap = GetLayerMipmap(relative_address);
        if (!layer_mipmap) {
            return {};
        }
        const u32 layer = (*layer_mipmap).first;
        const u32 mipmap = (*layer_mipmap).second;
        if (GetMipmapSize(mipmap) != size) {
            // TODO: the view may cover many mimaps, this case can still go on
            return {};
        }
        ViewParams vp{};
        vp.base_layer = layer;
        vp.num_layers = 1;
        vp.base_level = mipmap;
        vp.num_levels = 1;
        vp.target = params.target;
        return {GetView(vp)};
    }

    TView GetMainView() const {
        return main_view;
    }

protected:
    explicit SurfaceBase(const GPUVAddr gpu_addr, const SurfaceParams& params)
        : SurfaceBaseImpl(gpu_addr, params) {}

    ~SurfaceBase() = default;

    virtual TView CreateView(const ViewParams& view_key) = 0;

    std::unordered_map<ViewParams, TView> views;
    TView main_view;

private:
    TView GetView(const ViewParams& key) {
        const auto [entry, is_cache_miss] = views.try_emplace(key);
        auto& view{entry->second};
        if (is_cache_miss) {
            view = CreateView(key);
        }
        return view;
    }

    bool is_modified{};
    bool is_protected{};
    bool is_registered{};
    bool is_picked{};
    u64 modification_tick{};
};

} // namespace VideoCommon
