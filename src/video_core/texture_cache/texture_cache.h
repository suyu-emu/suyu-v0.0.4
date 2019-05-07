// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <memory>
#include <set>
#include <tuple>
#include <unordered_map>

#include <boost/icl/interval_map.hpp>
#include <boost/range/iterator_range.hpp>

#include "common/assert.h"
#include "common/common_types.h"
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
        for (const auto& surface : GetSurfacesInRegion(addr, size)) {
            Unregister(surface);
        }
    }

    void InvalidateRegionEx(GPUVAddr addr, std::size_t size) {
        for (const auto& surface : GetSurfacesInRegionInner(addr, size)) {
            Unregister(surface);
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
        const auto& regs{system.GPU().Maxwell3D().regs};
        const auto gpu_addr{regs.zeta.Address()};
        if (!gpu_addr || !regs.zeta_enable) {
            return {};
        }
        const auto depth_params{SurfaceParams::CreateForDepthBuffer(
            system, regs.zeta_width, regs.zeta_height, regs.zeta.format,
            regs.zeta.memory_layout.block_width, regs.zeta.memory_layout.block_height,
            regs.zeta.memory_layout.block_depth, regs.zeta.memory_layout.type)};
        auto surface_view = GetSurface(gpu_addr, depth_params, preserve_contents);
        if (depth_buffer.target)
            depth_buffer.target->MarkAsProtected(false);
        if (depth_buffer.target)
            depth_buffer.target->MarkAsProtected(true);
        return surface_view.second;
    }

    TView GetColorBufferSurface(std::size_t index, bool preserve_contents) {
        ASSERT(index < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets);

        const auto& regs{system.GPU().Maxwell3D().regs};
        if (index >= regs.rt_control.count || regs.rt[index].Address() == 0 ||
            regs.rt[index].format == Tegra::RenderTargetFormat::NONE) {
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
            render_targets[index].target->MarkAsProtected(false);
        render_targets[index].target = surface_view.first;
        if (render_targets[index].target)
            render_targets[index].target->MarkAsProtected(true);
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
            depth_buffer.target->MarkAsProtected(false);
            depth_buffer.target = nullptr;
            depth_buffer.view = nullptr;
        }
    }

    void SetEmptyColorBuffer(std::size_t index) {
        if (render_targets[index].target != nullptr) {
            render_targets[index].target->MarkAsProtected(false);
            std::memset(&render_targets[index].config, sizeof(RenderTargetConfig), 0);
            render_targets[index].target = nullptr;
            render_targets[index].view = nullptr;
        }
    }

    TView GetFermiSurface(const Tegra::Engines::Fermi2D::Regs::Surface& config) {
        SurfaceParams params = SurfaceParams::CreateForFermiCopySurface(config);
        const GPUVAddr gpu_addr = config.Address();
        return GetSurface(gpu_addr, params, true).second;
    }

    TSurface TryFindFramebufferSurface(const u8* host_ptr) const {
        const auto it{registered_surfaces.find(ToCacheAddr(host_ptr))};
        return it != registered_surfaces.end() ? *it->second.begin() : nullptr;
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

    void Register(TSurface surface) {
        const GPUVAddr gpu_addr = surface->GetGpuAddr();
        u8* host_ptr = memory_manager->GetPointer(gpu_addr);
        const std::size_t size = surface->GetSizeInBytes();
        const std::optional<VAddr> cpu_addr = memory_manager->GpuToCpuAddress(gpu_addr);
        if (!host_ptr || !cpu_addr) {
            LOG_CRITICAL(HW_GPU, "Failed to register surface with unmapped gpu_address 0x{:016x}",
                         gpu_addr);
            return;
        }
        surface->SetHostPtr(host_ptr);
        surface->SetCpuAddr(*cpu_addr);
        registered_surfaces.add({GetInterval(host_ptr, size), {surface}});
        rasterizer.UpdatePagesCachedCount(*cpu_addr, size, 1);
        RegisterInnerCache(surface);
        surface->MarkAsRegistered(true);
    }

    void Unregister(TSurface surface) {
        if (surface->IsProtected())
            return;
        const GPUVAddr gpu_addr = surface->GetGpuAddr();
        const void* host_ptr = surface->GetHostPtr();
        const std::size_t size = surface->GetSizeInBytes();
        const VAddr cpu_addr = surface->GetCpuAddr();
        registered_surfaces.erase(GetInterval(host_ptr, size));
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

    Core::System& system;

private:
    enum class RecycleStrategy : u32 {
        Ignore = 0,
        Flush = 1,
        BufferCopy = 3,
    };

    RecycleStrategy PickStrategy(std::vector<TSurface>& overlaps, const SurfaceParams& params,
                                 const GPUVAddr gpu_addr, const bool untopological) {
        // Untopological decision
        if (untopological) {
            return RecycleStrategy::Ignore;
        }
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
        return RecycleStrategy::Ignore;
    }

    std::pair<TSurface, TView> RecycleSurface(std::vector<TSurface>& overlaps,
                                              const SurfaceParams& params, const GPUVAddr gpu_addr,
                                              const u8* host_ptr, const bool preserve_contents,
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

    std::pair<TSurface, TView> RebuildMirage(TSurface current_surface,
                                             const SurfaceParams& params) {
        const auto gpu_addr = current_surface->GetGpuAddr();
        TSurface new_surface = GetUncachedSurface(gpu_addr, params);
        std::vector<CopyParams> bricks = current_surface->BreakDown();
        for (auto& brick : bricks) {
            ImageCopy(current_surface, new_surface, brick);
        }
        Unregister(current_surface);
        Register(new_surface);
        return {new_surface, new_surface->GetMainView()};
    }

    std::pair<TSurface, TView> ManageStructuralMatch(TSurface current_surface,
                                                     const SurfaceParams& params) {
        const bool is_mirage = !current_surface->MatchFormat(params.pixel_format);
        if (is_mirage) {
            return RebuildMirage(current_surface, params);
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
        if (!params.is_layered || params.target == SurfaceTarget::Texture3D) {
            return {};
        }
        TSurface new_surface = GetUncachedSurface(gpu_addr, params);
        for (auto surface : overlaps) {
            const SurfaceParams& src_params = surface->GetSurfaceParams();
            if (src_params.is_layered || src_params.num_levels > 1) {
                // We send this cases to recycle as they are more complex to handle
                return {};
            }
            const std::size_t candidate_size = src_params.GetGuestSizeInBytes();
            auto mipmap_layer = new_surface->GetLayerMipmap(surface->GetGpuAddr());
            if (!mipmap_layer) {
                return {};
            }
            const u32 layer = (*mipmap_layer).first;
            const u32 mipmap = (*mipmap_layer).second;
            if (new_surface->GetMipmapSize(mipmap) != candidate_size) {
                return {};
            }
            // Now we got all the data set up
            CopyParams copy_params{};
            const u32 dst_width = params.GetMipWidth(mipmap);
            const u32 dst_height = params.GetMipHeight(mipmap);
            copy_params.width = std::min(src_params.width, dst_width);
            copy_params.height = std::min(src_params.height, dst_height);
            copy_params.depth = 1;
            copy_params.source_level = 0;
            copy_params.dest_level = mipmap;
            copy_params.source_z = 0;
            copy_params.dest_z = layer;
            ImageCopy(surface, new_surface, copy_params);
        }
        for (auto surface : overlaps) {
            Unregister(surface);
        }
        Register(new_surface);
        return {{new_surface, new_surface->GetMainView()}};
    }

    std::pair<TSurface, TView> GetSurface(const GPUVAddr gpu_addr, const SurfaceParams& params,
                                          bool preserve_contents) {

        const auto host_ptr{memory_manager->GetPointer(gpu_addr)};
        const auto cache_addr{ToCacheAddr(host_ptr)};
        const std::size_t candidate_size = params.GetGuestSizeInBytes();
        auto overlaps{GetSurfacesInRegionInner(gpu_addr, candidate_size)};
        if (overlaps.empty()) {
            return InitializeSurface(gpu_addr, params, preserve_contents);
        }

        for (auto surface : overlaps) {
            if (!surface->MatchesTopology(params)) {
                return RecycleSurface(overlaps, params, gpu_addr, host_ptr, preserve_contents,
                                      true);
            }
        }

        if (overlaps.size() == 1) {
            TSurface current_surface = overlaps[0];
            if (current_surface->MatchesStructure(params) &&
                current_surface->GetGpuAddr() == gpu_addr &&
                (params.target != SurfaceTarget::Texture3D ||
                 current_surface->MatchTarget(params.target))) {
                return ManageStructuralMatch(current_surface, params);
            }
            if (current_surface->GetSizeInBytes() <= candidate_size) {
                return RecycleSurface(overlaps, params, gpu_addr, host_ptr, preserve_contents,
                                      false);
            }
            std::optional<TView> view = current_surface->EmplaceView(params, gpu_addr);
            if (view.has_value()) {
                const bool is_mirage = !current_surface->MatchFormat(params.pixel_format);
                if (is_mirage) {
                    LOG_CRITICAL(HW_GPU, "Mirage View Unsupported");
                    return RecycleSurface(overlaps, params, gpu_addr, host_ptr, preserve_contents,
                                          false);
                }
                return {current_surface, *view};
            }
            return RecycleSurface(overlaps, params, gpu_addr, host_ptr, preserve_contents, false);
        } else {
            std::optional<std::pair<TSurface, TView>> view =
                ReconstructSurface(overlaps, params, gpu_addr, host_ptr);
            if (view.has_value()) {
                return *view;
            }
            return RecycleSurface(overlaps, params, gpu_addr, host_ptr, preserve_contents, false);
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
        surface->FlushBuffer(staging_buffer);
        surface->MarkAsModified(false, Tick());
    }

    std::vector<TSurface> GetSurfacesInRegion(CacheAddr cache_addr, std::size_t size) const {
        if (size == 0) {
            return {};
        }
        const IntervalType interval{cache_addr, cache_addr + size};

        std::vector<TSurface> surfaces;
        for (auto& pair : boost::make_iterator_range(registered_surfaces.equal_range(interval))) {
            for (auto& s : pair.second) {
                if (!s || !s->IsRegistered()) {
                    continue;
                }
                surfaces.push_back(s);
            }
        }
        return surfaces;
    }

    void RegisterInnerCache(TSurface& surface) {
        GPUVAddr start = surface->GetGpuAddr() >> inner_cache_page_bits;
        const GPUVAddr end = (surface->GetGpuAddrEnd() - 1) >> inner_cache_page_bits;
        while (start <= end) {
            inner_cache[start].push_back(surface);
            start++;
        }
    }

    void UnregisterInnerCache(TSurface& surface) {
        GPUVAddr start = surface->GetGpuAddr() >> inner_cache_page_bits;
        const GPUVAddr end = (surface->GetGpuAddrEnd() - 1)  >> inner_cache_page_bits;
        while (start <= end) {
            inner_cache[start].remove(surface);
            start++;
        }
    }

    std::vector<TSurface> GetSurfacesInRegionInner(const GPUVAddr gpu_addr, const std::size_t size) {
        if (size == 0) {
            return {};
        }
        const GPUVAddr gpu_addr_end = gpu_addr + size;
        GPUVAddr start = gpu_addr >> inner_cache_page_bits;
        const GPUVAddr end = (gpu_addr_end - 1) >> inner_cache_page_bits;
        std::vector<TSurface> surfaces;
        while (start <= end) {
            std::list<TSurface>& list = inner_cache[start];
            for (auto& s : list) {
                if (!s->IsPicked() && s->Overlaps(gpu_addr, gpu_addr_end)) {
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

    IntervalType GetInterval(const void* host_ptr, const std::size_t size) const {
        const CacheAddr addr = ToCacheAddr(host_ptr);
        return IntervalType::right_open(addr, addr + size);
    }

    struct RenderInfo {
        RenderTargetConfig config;
        TSurface target;
        TView view;
    };

    struct DepthBufferInfo {
        TSurface target;
        TView view;
    };

    VideoCore::RasterizerInterface& rasterizer;
    Tegra::MemoryManager* memory_manager;

    u64 ticks{};

    IntervalMap registered_surfaces;

    static constexpr u64 inner_cache_page_bits{20};
    static constexpr u64 inner_cache_page_size{1 << inner_cache_page_bits};
    std::unordered_map<GPUVAddr, std::list<TSurface>> inner_cache;

    /// The surface reserve is a "backup" cache, this is where we put unique surfaces that have
    /// previously been used. This is to prevent surfaces from being constantly created and
    /// destroyed when used with different surface parameters.
    std::unordered_map<SurfaceParams, std::list<TSurface>> surface_reserve;
    std::array<RenderInfo, Tegra::Engines::Maxwell3D::Regs::NumRenderTargets> render_targets;
    DepthBufferInfo depth_buffer;

    std::vector<u8> staging_buffer;
};

} // namespace VideoCommon
