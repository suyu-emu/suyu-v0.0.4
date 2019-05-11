// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <mutex>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

#include <boost/icl/interval_map.hpp>
#include <boost/range/iterator_range.hpp>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/memory.h"
#include "video_core/engines/fermi_2d.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/gpu.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/surface.h"
#include "video_core/texture_cache/copy_params.h"
#include "video_core/texture_cache/surface_base.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/texture_cache/surface_view.h"

namespace Core {
class System;
}

namespace Tegra::Texture {
struct FullTextureInfo;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace VideoCommon {

using VideoCore::Surface::SurfaceTarget;
using RenderTargetConfig = Tegra::Engines::Maxwell3D::Regs::RenderTargetConfig;

template <typename TSurface, typename TView>
class TextureCache {
    using IntervalMap = boost::icl::interval_map<CacheAddr, std::set<TSurface>>;
    using IntervalType = typename IntervalMap::interval_type;

public:
    void InitMemoryMananger(Tegra::MemoryManager& memory_manager) {
        this->memory_manager = &memory_manager;
    }

    void InvalidateRegion(CacheAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        for (const auto& surface : GetSurfacesInRegion(addr, size)) {
            Unregister(surface);
        }
    }

    void FlushRegion(CacheAddr addr, std::size_t size) {
        std::lock_guard lock{mutex};

        auto surfaces = GetSurfacesInRegion(addr, size);
        if (surfaces.empty()) {
            return;
        }
        std::sort(surfaces.begin(), surfaces.end(),
                  [](const TSurface& a, const TSurface& b) -> bool {
                      return a->GetModificationTick() < b->GetModificationTick();
                  });
        for (const auto& surface : surfaces) {
            FlushSurface(surface);
        }
    }

    TView GetTextureSurface(const Tegra::Texture::FullTextureInfo& config,
                            const VideoCommon::Shader::Sampler& entry) {
        const auto gpu_addr{config.tic.Address()};
        if (!gpu_addr) {
            return {};
        }
        const auto params{SurfaceParams::CreateForTexture(system, config, entry)};
        return GetSurface(gpu_addr, params, true).second;
    }

    TView GetDepthBufferSurface(bool preserve_contents) {
        auto& maxwell3d = system.GPU().Maxwell3D();

        if (!maxwell3d.dirty_flags.zeta_buffer) {
            return depth_buffer.view;
        }
        maxwell3d.dirty_flags.zeta_buffer = false;

        const auto& regs{maxwell3d.regs};
        const auto gpu_addr{regs.zeta.Address()};
        if (!gpu_addr || !regs.zeta_enable) {
            SetEmptyDepthBuffer();
            return {};
        }
        const auto depth_params{SurfaceParams::CreateForDepthBuffer(
            system, regs.zeta_width, regs.zeta_height, regs.zeta.format,
            regs.zeta.memory_layout.block_width, regs.zeta.memory_layout.block_height,
            regs.zeta.memory_layout.block_depth, regs.zeta.memory_layout.type)};
        auto surface_view = GetSurface(gpu_addr, depth_params, preserve_contents);
        if (depth_buffer.target)
            depth_buffer.target->MarkAsRenderTarget(false);
        depth_buffer.target = surface_view.first;
        depth_buffer.view = surface_view.second;
        if (depth_buffer.target)
            depth_buffer.target->MarkAsRenderTarget(true);
        return surface_view.second;
    }

    TView GetColorBufferSurface(std::size_t index, bool preserve_contents) {
        ASSERT(index < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets);
        auto& maxwell3d = system.GPU().Maxwell3D();
        if (!maxwell3d.dirty_flags.color_buffer[index]) {
            return render_targets[index].view;
        }
        maxwell3d.dirty_flags.color_buffer.reset(index);

        const auto& regs{maxwell3d.regs};
        if (index >= regs.rt_control.count || regs.rt[index].Address() == 0 ||
            regs.rt[index].format == Tegra::RenderTargetFormat::NONE) {
            SetEmptyColorBuffer(index);
            return {};
        }

        if (regs.color_mask[index].raw != 0) {
            SetEmptyColorBuffer(index);
            return {};
        }

        const auto& config{regs.rt[index]};
        const auto gpu_addr{config.Address()};
        if (!gpu_addr) {
            SetEmptyColorBuffer(index);
            return {};
        }

        auto surface_view = GetSurface(gpu_addr, SurfaceParams::CreateForFramebuffer(system, index),
                                       preserve_contents);
        if (render_targets[index].target)
            render_targets[index].target->MarkAsRenderTarget(false);
        render_targets[index].target = surface_view.first;
        render_targets[index].view = surface_view.second;
        if (render_targets[index].target)
            render_targets[index].target->MarkAsRenderTarget(true);
        return surface_view.second;
    }

    void MarkColorBufferInUse(std::size_t index) {
        if (render_targets[index].target)
            render_targets[index].target->MarkAsModified(true, Tick());
    }

    void MarkDepthBufferInUse() {
        if (depth_buffer.target)
            depth_buffer.target->MarkAsModified(true, Tick());
    }

    void SetEmptyDepthBuffer() {
        if (depth_buffer.target != nullptr) {
            depth_buffer.target->MarkAsRenderTarget(false);
            depth_buffer.target = nullptr;
            depth_buffer.view = nullptr;
        }
    }

    void SetEmptyColorBuffer(std::size_t index) {
        if (render_targets[index].target != nullptr) {
            render_targets[index].target->MarkAsRenderTarget(false);
            render_targets[index].target = nullptr;
            render_targets[index].view = nullptr;
        }
    }

    void DoFermiCopy(const Tegra::Engines::Fermi2D::Regs::Surface& src_config,
                     const Tegra::Engines::Fermi2D::Regs::Surface& dst_config,
                     const Common::Rectangle<u32>& src_rect,
                     const Common::Rectangle<u32>& dst_rect) {
        TSurface dst_surface = GetFermiSurface(dst_config);
        ImageBlit(GetFermiSurface(src_config), dst_surface, src_rect, dst_rect);
        dst_surface->MarkAsModified(true, Tick());
    }

    TSurface TryFindFramebufferSurface(const u8* host_ptr) {
        const CacheAddr cache_addr = ToCacheAddr(host_ptr);
        if (!cache_addr) {
            return nullptr;
        }
        const CacheAddr page = cache_addr >> registry_page_bits;
        std::vector<TSurface>& list = registry[page];
        for (auto& s : list) {
            if (s->GetCacheAddr() == cache_addr) {
                return s;
            }
        }
        return nullptr;
    }

    u64 Tick() {
        return ++ticks;
    }

protected:
    TextureCache(Core::System& system, VideoCore::RasterizerInterface& rasterizer)
        : system{system}, rasterizer{rasterizer} {
        for (std::size_t i = 0; i < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets; i++) {
            SetEmptyColorBuffer(i);
        }
        SetEmptyDepthBuffer();
    }

    ~TextureCache() = default;

    virtual TSurface CreateSurface(GPUVAddr gpu_addr, const SurfaceParams& params) = 0;

    virtual void ImageCopy(TSurface src_surface, TSurface dst_surface,
                           const CopyParams& copy_params) = 0;

    virtual void ImageBlit(TSurface src, TSurface dst, const Common::Rectangle<u32>& src_rect,
                           const Common::Rectangle<u32>& dst_rect) = 0;

    void Register(TSurface surface) {
        std::lock_guard lock{mutex};

        const GPUVAddr gpu_addr = surface->GetGpuAddr();
        const CacheAddr cache_ptr = ToCacheAddr(memory_manager->GetPointer(gpu_addr));
        const std::size_t size = surface->GetSizeInBytes();
        const std::optional<VAddr> cpu_addr = memory_manager->GpuToCpuAddress(gpu_addr);
        if (!cache_ptr || !cpu_addr) {
            LOG_CRITICAL(HW_GPU, "Failed to register surface with unmapped gpu_address 0x{:016x}",
                         gpu_addr);
            return;
        }
        surface->SetCacheAddr(cache_ptr);
        surface->SetCpuAddr(*cpu_addr);
        RegisterInnerCache(surface);
        surface->MarkAsRegistered(true);
        rasterizer.UpdatePagesCachedCount(*cpu_addr, size, 1);
    }

    void Unregister(TSurface surface) {
        std::lock_guard lock{mutex};

        if (surface->IsProtected()) {
            return;
        }
        const GPUVAddr gpu_addr = surface->GetGpuAddr();
        const CacheAddr cache_ptr = surface->GetCacheAddr();
        const std::size_t size = surface->GetSizeInBytes();
        const VAddr cpu_addr = surface->GetCpuAddr();
        rasterizer.UpdatePagesCachedCount(cpu_addr, size, -1);
        UnregisterInnerCache(surface);
        surface->MarkAsRegistered(false);
        ReserveSurface(surface->GetSurfaceParams(), surface);
    }

    TSurface GetUncachedSurface(const GPUVAddr gpu_addr, const SurfaceParams& params) {
        if (const auto surface = TryGetReservedSurface(params); surface) {
            surface->SetGpuAddr(gpu_addr);
            return surface;
        }
        // No reserved surface available, create a new one and reserve it
        auto new_surface{CreateSurface(gpu_addr, params)};
        return new_surface;
    }

    TSurface GetFermiSurface(const Tegra::Engines::Fermi2D::Regs::Surface& config) {
        SurfaceParams params = SurfaceParams::CreateForFermiCopySurface(config);
        const GPUVAddr gpu_addr = config.Address();
        return GetSurface(gpu_addr, params, true).first;
    }

    Core::System& system;

private:
    enum class RecycleStrategy : u32 {
        Ignore = 0,
        Flush = 1,
        BufferCopy = 3,
    };

    RecycleStrategy PickStrategy(std::vector<TSurface>& overlaps, const SurfaceParams& params,
                                 const GPUVAddr gpu_addr, const bool untopological) {
        // 3D Textures decision
        if (params.block_depth > 1 || params.target == SurfaceTarget::Texture3D) {
            return RecycleStrategy::Flush;
        }
        for (auto s : overlaps) {
            const auto& s_params = s->GetSurfaceParams();
            if (s_params.block_depth > 1 || s_params.target == SurfaceTarget::Texture3D) {
                return RecycleStrategy::Flush;
            }
        }
        // Untopological decision
        if (untopological) {
            return RecycleStrategy::Ignore;
        }
        return RecycleStrategy::Ignore;
    }

    std::pair<TSurface, TView> RecycleSurface(std::vector<TSurface>& overlaps,
                                              const SurfaceParams& params, const GPUVAddr gpu_addr,
                                              const bool preserve_contents,
                                              const bool untopological) {
        for (auto surface : overlaps) {
            Unregister(surface);
        }
        RecycleStrategy strategy = !Settings::values.use_accurate_gpu_emulation
                                       ? PickStrategy(overlaps, params, gpu_addr, untopological)
                                       : RecycleStrategy::Flush;
        switch (strategy) {
        case RecycleStrategy::Ignore: {
            return InitializeSurface(gpu_addr, params, preserve_contents);
        }
        case RecycleStrategy::Flush: {
            std::sort(overlaps.begin(), overlaps.end(),
                      [](const TSurface& a, const TSurface& b) -> bool {
                          return a->GetModificationTick() < b->GetModificationTick();
                      });
            for (auto surface : overlaps) {
                FlushSurface(surface);
            }
            return InitializeSurface(gpu_addr, params, preserve_contents);
        }
        default: {
            UNIMPLEMENTED_MSG("Unimplemented Texture Cache Recycling Strategy!");
            return InitializeSurface(gpu_addr, params, preserve_contents);
        }
        }
    }

    std::pair<TSurface, TView> RebuildSurface(TSurface current_surface,
                                              const SurfaceParams& params) {
        const auto gpu_addr = current_surface->GetGpuAddr();
        TSurface new_surface = GetUncachedSurface(gpu_addr, params);
        std::vector<CopyParams> bricks = current_surface->BreakDown(params);
        for (auto& brick : bricks) {
            ImageCopy(current_surface, new_surface, brick);
        }
        Unregister(current_surface);
        Register(new_surface);
        new_surface->MarkAsModified(current_surface->IsModified(), Tick());
        return {new_surface, new_surface->GetMainView()};
    }

    std::pair<TSurface, TView> ManageStructuralMatch(TSurface current_surface,
                                                     const SurfaceParams& params) {
        const bool is_mirage = !current_surface->MatchFormat(params.pixel_format);
        if (is_mirage) {
            return RebuildSurface(current_surface, params);
        }
        const bool matches_target = current_surface->MatchTarget(params.target);
        if (matches_target) {
            return {current_surface, current_surface->GetMainView()};
        }
        return {current_surface, current_surface->EmplaceOverview(params)};
    }

    std::optional<std::pair<TSurface, TView>> ReconstructSurface(std::vector<TSurface>& overlaps,
                                                                 const SurfaceParams& params,
                                                                 const GPUVAddr gpu_addr,
                                                                 const u8* host_ptr) {
        if (params.target == SurfaceTarget::Texture3D) {
            return {};
        }
        bool modified = false;
        TSurface new_surface = GetUncachedSurface(gpu_addr, params);
        for (auto surface : overlaps) {
            const SurfaceParams& src_params = surface->GetSurfaceParams();
            if (src_params.is_layered || src_params.num_levels > 1) {
                // We send this cases to recycle as they are more complex to handle
                return {};
            }
            const std::size_t candidate_size = surface->GetSizeInBytes();
            auto mipmap_layer{new_surface->GetLayerMipmap(surface->GetGpuAddr())};
            if (!mipmap_layer) {
                return {};
            }
            const u32 layer{mipmap_layer->first};
            const u32 mipmap{mipmap_layer->second};
            if (new_surface->GetMipmapSize(mipmap) != candidate_size) {
                return {};
            }
            modified |= surface->IsModified();
            // Now we got all the data set up
            const u32 dst_width{params.GetMipWidth(mipmap)};
            const u32 dst_height{params.GetMipHeight(mipmap)};
            const CopyParams copy_params(0, 0, 0, 0, 0, layer, 0, mipmap,
                                         std::min(src_params.width, dst_width),
                                         std::min(src_params.height, dst_height), 1);
            ImageCopy(surface, new_surface, copy_params);
        }
        for (auto surface : overlaps) {
            Unregister(surface);
        }
        new_surface->MarkAsModified(modified, Tick());
        Register(new_surface);
        return {{new_surface, new_surface->GetMainView()}};
    }

    std::pair<TSurface, TView> GetSurface(const GPUVAddr gpu_addr, const SurfaceParams& params,
                                          bool preserve_contents) {

        const auto host_ptr{memory_manager->GetPointer(gpu_addr)};
        const auto cache_addr{ToCacheAddr(host_ptr)};

        if (l1_cache.count(cache_addr) > 0) {
            TSurface current_surface = l1_cache[cache_addr];
            if (!current_surface->MatchesTopology(params)) {
                std::vector<TSurface> overlaps{current_surface};
                return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, true);
            }
            MatchStructureResult s_result = current_surface->MatchesStructure(params);
            if (s_result != MatchStructureResult::None &&
                current_surface->GetGpuAddr() == gpu_addr &&
                (params.target != SurfaceTarget::Texture3D ||
                 current_surface->MatchTarget(params.target))) {
                if (s_result == MatchStructureResult::FullMatch) {
                    return ManageStructuralMatch(current_surface, params);
                } else {
                    return RebuildSurface(current_surface, params);
                }
            }
        }

        const std::size_t candidate_size = params.GetGuestSizeInBytes();
        auto overlaps{GetSurfacesInRegion(cache_addr, candidate_size)};

        if (overlaps.empty()) {
            return InitializeSurface(gpu_addr, params, preserve_contents);
        }

        for (auto surface : overlaps) {
            if (!surface->MatchesTopology(params)) {
                return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, true);
            }
        }

        if (overlaps.size() == 1) {
            TSurface current_surface = overlaps[0];
            if (!current_surface->IsInside(gpu_addr, gpu_addr + candidate_size)) {
                return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, false);
            }
            std::optional<TView> view =
                current_surface->EmplaceView(params, gpu_addr, candidate_size);
            if (view.has_value()) {
                const bool is_mirage = !current_surface->MatchFormat(params.pixel_format);
                if (is_mirage) {
                    LOG_CRITICAL(HW_GPU, "Mirage View Unsupported");
                    return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, false);
                }
                return {current_surface, *view};
            }
            return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, false);
        } else {
            std::optional<std::pair<TSurface, TView>> view =
                ReconstructSurface(overlaps, params, gpu_addr, host_ptr);
            if (view.has_value()) {
                return *view;
            }
            return RecycleSurface(overlaps, params, gpu_addr, preserve_contents, false);
        }
    }

    std::pair<TSurface, TView> InitializeSurface(GPUVAddr gpu_addr, const SurfaceParams& params,
                                                 bool preserve_contents) {
        auto new_surface{GetUncachedSurface(gpu_addr, params)};
        Register(new_surface);
        if (preserve_contents) {
            LoadSurface(new_surface);
        }
        return {new_surface, new_surface->GetMainView()};
    }

    void LoadSurface(const TSurface& surface) {
        staging_buffer.resize(surface->GetHostSizeInBytes());
        surface->LoadBuffer(*memory_manager, staging_buffer);
        surface->UploadTexture(staging_buffer);
        surface->MarkAsModified(false, Tick());
    }

    void FlushSurface(const TSurface& surface) {
        if (!surface->IsModified()) {
            return;
        }
        staging_buffer.resize(surface->GetHostSizeInBytes());
        surface->DownloadTexture(staging_buffer);
        surface->FlushBuffer(*memory_manager, staging_buffer);
        surface->MarkAsModified(false, Tick());
    }

    void RegisterInnerCache(TSurface& surface) {
        const CacheAddr cache_addr = surface->GetCacheAddr();
        CacheAddr start = cache_addr >> registry_page_bits;
        const CacheAddr end = (surface->GetCacheAddrEnd() - 1) >> registry_page_bits;
        l1_cache[cache_addr] = surface;
        while (start <= end) {
            registry[start].push_back(surface);
            start++;
        }
    }

    void UnregisterInnerCache(TSurface& surface) {
        const CacheAddr cache_addr = surface->GetCacheAddr();
        CacheAddr start = cache_addr >> registry_page_bits;
        const CacheAddr end = (surface->GetCacheAddrEnd() - 1) >> registry_page_bits;
        l1_cache.erase(cache_addr);
        while (start <= end) {
            auto& reg{registry[start]};
            reg.erase(std::find(reg.begin(), reg.end(), surface));
            start++;
        }
    }

    std::vector<TSurface> GetSurfacesInRegion(const CacheAddr cache_addr, const std::size_t size) {
        if (size == 0) {
            return {};
        }
        const CacheAddr cache_addr_end = cache_addr + size;
        CacheAddr start = cache_addr >> registry_page_bits;
        const CacheAddr end = (cache_addr_end - 1) >> registry_page_bits;
        std::vector<TSurface> surfaces;
        while (start <= end) {
            std::vector<TSurface>& list = registry[start];
            for (auto& s : list) {
                if (!s->IsPicked() && s->Overlaps(cache_addr, cache_addr_end)) {
                    s->MarkAsPicked(true);
                    surfaces.push_back(s);
                }
            }
            start++;
        }
        for (auto& s : surfaces) {
            s->MarkAsPicked(false);
        }
        return surfaces;
    }

    void ReserveSurface(const SurfaceParams& params, TSurface surface) {
        surface_reserve[params].push_back(std::move(surface));
    }

    TSurface TryGetReservedSurface(const SurfaceParams& params) {
        auto search{surface_reserve.find(params)};
        if (search == surface_reserve.end()) {
            return {};
        }
        for (auto& surface : search->second) {
            if (!surface->IsRegistered()) {
                return surface;
            }
        }
        return {};
    }

    struct FramebufferTargetInfo {
        TSurface target;
        TView view;
    };

    VideoCore::RasterizerInterface& rasterizer;
    Tegra::MemoryManager* memory_manager;

    u64 ticks{};

    // The internal Cache is different for the Texture Cache. It's based on buckets
    // of 1MB. This fits better for the purpose of this cache as textures are normaly
    // large in size.
    static constexpr u64 registry_page_bits{20};
    static constexpr u64 registry_page_size{1 << registry_page_bits};
    std::unordered_map<CacheAddr, std::vector<TSurface>> registry;

    // The L1 Cache is used for fast texture lookup before checking the overlaps
    // This avoids calculating size and other stuffs.
    std::unordered_map<CacheAddr, TSurface> l1_cache;

    /// The surface reserve is a "backup" cache, this is where we put unique surfaces that have
    /// previously been used. This is to prevent surfaces from being constantly created and
    /// destroyed when used with different surface parameters.
    std::unordered_map<SurfaceParams, std::vector<TSurface>> surface_reserve;
    std::array<FramebufferTargetInfo, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets>
        render_targets;
    FramebufferTargetInfo depth_buffer;

    std::vector<u8> staging_buffer;
    std::recursive_mutex mutex;
};

} // namespace VideoCommon
