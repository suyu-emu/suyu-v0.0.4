// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <list>
#include <memory>
#include <set>
#include <tuple>
#include <type_traits>
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

template <typename TSurface, typename TView, typename TExecutionContext>
class TextureCache {
    static_assert(std::is_trivially_copyable_v<TExecutionContext>);

    using ResultType = std::tuple<TView*, TExecutionContext>;
    using IntervalMap = boost::icl::interval_map<CacheAddr, std::set<std::shared_ptr<TSurface>>>;
    using IntervalType = typename IntervalMap::interval_type;

public:
    void InvalidateRegion(CacheAddr addr, std::size_t size) {
        for (const auto& surface : GetSurfacesInRegion(addr, size)) {
            if (!surface->IsRegistered()) {
                // Skip duplicates
                continue;
            }
            Unregister(surface);
        }
    }

    ResultType GetTextureSurface(TExecutionContext exctx,
                                 const Tegra::Texture::FullTextureInfo& config) {
        const auto gpu_addr{config.tic.Address()};
        if (!gpu_addr) {
            return {{}, exctx};
        }
        const auto params{SurfaceParams::CreateForTexture(system, config)};
        return GetSurfaceView(exctx, gpu_addr, params, true);
    }

    ResultType GetDepthBufferSurface(TExecutionContext exctx, bool preserve_contents) {
        const auto& regs{system.GPU().Maxwell3D().regs};
        const auto gpu_addr{regs.zeta.Address()};
        if (!gpu_addr || !regs.zeta_enable) {
            return {{}, exctx};
        }
        const auto depth_params{SurfaceParams::CreateForDepthBuffer(
            system, regs.zeta_width, regs.zeta_height, regs.zeta.format,
            regs.zeta.memory_layout.block_width, regs.zeta.memory_layout.block_height,
            regs.zeta.memory_layout.block_depth, regs.zeta.memory_layout.type)};
        return GetSurfaceView(exctx, gpu_addr, depth_params, preserve_contents);
    }

    ResultType GetColorBufferSurface(TExecutionContext exctx, std::size_t index,
                                     bool preserve_contents) {
        ASSERT(index < Tegra::Engines::Maxwell3D::Regs::NumRenderTargets);

        const auto& regs{system.GPU().Maxwell3D().regs};
        if (index >= regs.rt_control.count || regs.rt[index].Address() == 0 ||
            regs.rt[index].format == Tegra::RenderTargetFormat::NONE) {
            return {{}, exctx};
        }

        auto& memory_manager{system.GPU().MemoryManager()};
        const auto& config{system.GPU().Maxwell3D().regs.rt[index]};
        const auto gpu_addr{config.Address() +
                            config.base_layer * config.layer_stride * sizeof(u32)};
        if (!gpu_addr) {
            return {{}, exctx};
        }

        return GetSurfaceView(exctx, gpu_addr, SurfaceParams::CreateForFramebuffer(system, index),
                              preserve_contents);
    }

    ResultType GetFermiSurface(TExecutionContext exctx,
                               const Tegra::Engines::Fermi2D::Regs::Surface& config) {
        return GetSurfaceView(exctx, config.Address(),
                              SurfaceParams::CreateForFermiCopySurface(config), true);
    }

    std::shared_ptr<TSurface> TryFindFramebufferSurface(const u8* host_ptr) const {
        const auto it{registered_surfaces.find(ToCacheAddr(host_ptr))};
        return it != registered_surfaces.end() ? *it->second.begin() : nullptr;
    }

    u64 Tick() {
        return ++ticks;
    }

protected:
    TextureCache(Core::System& system, VideoCore::RasterizerInterface& rasterizer)
        : system{system}, rasterizer{rasterizer} {}

    ~TextureCache() = default;

    virtual ResultType TryFastGetSurfaceView(
        TExecutionContext exctx, GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr,
        const SurfaceParams& params, bool preserve_contents,
        const std::vector<std::shared_ptr<TSurface>>& overlaps) = 0;

    virtual std::shared_ptr<TSurface> CreateSurface(const SurfaceParams& params) = 0;

    void Register(std::shared_ptr<TSurface> surface, GPUVAddr gpu_addr, VAddr cpu_addr,
                  u8* host_ptr) {
        surface->Register(gpu_addr, cpu_addr, host_ptr);
        registered_surfaces.add({GetSurfaceInterval(surface), {surface}});
        rasterizer.UpdatePagesCachedCount(surface->GetCpuAddr(), surface->GetSizeInBytes(), 1);
    }

    void Unregister(std::shared_ptr<TSurface> surface) {
        registered_surfaces.subtract({GetSurfaceInterval(surface), {surface}});
        rasterizer.UpdatePagesCachedCount(surface->GetCpuAddr(), surface->GetSizeInBytes(), -1);
        surface->Unregister();
    }

    std::shared_ptr<TSurface> GetUncachedSurface(const SurfaceParams& params) {
        if (const auto surface = TryGetReservedSurface(params); surface)
            return surface;
        // No reserved surface available, create a new one and reserve it
        auto new_surface{CreateSurface(params)};
        ReserveSurface(params, new_surface);
        return new_surface;
    }

    Core::System& system;

private:
    ResultType GetSurfaceView(TExecutionContext exctx, GPUVAddr gpu_addr,
                              const SurfaceParams& params, bool preserve_contents) {
        auto& memory_manager{system.GPU().MemoryManager()};
        const auto cpu_addr{memory_manager.GpuToCpuAddress(gpu_addr)};
        DEBUG_ASSERT(cpu_addr);

        const auto host_ptr{memory_manager.GetPointer(gpu_addr)};
        const auto cache_addr{ToCacheAddr(host_ptr)};
        auto overlaps{GetSurfacesInRegion(cache_addr, params.GetGuestSizeInBytes())};
        if (overlaps.empty()) {
            return LoadSurfaceView(exctx, gpu_addr, *cpu_addr, host_ptr, params, preserve_contents);
        }

        if (overlaps.size() == 1) {
            if (TView* view = overlaps[0]->TryGetView(gpu_addr, params); view) {
                return {view, exctx};
            }
        }

        TView* fast_view;
        std::tie(fast_view, exctx) = TryFastGetSurfaceView(exctx, gpu_addr, *cpu_addr, host_ptr,
                                                           params, preserve_contents, overlaps);

        if (!fast_view) {
            std::sort(overlaps.begin(), overlaps.end(), [](const auto& lhs, const auto& rhs) {
                return lhs->GetModificationTick() < rhs->GetModificationTick();
            });
        }

        for (const auto& surface : overlaps) {
            if (!fast_view) {
                // Flush even when we don't care about the contents, to preserve memory not
                // written by the new surface.
                exctx = FlushSurface(exctx, surface);
            }
            Unregister(surface);
        }

        if (fast_view) {
            return {fast_view, exctx};
        }

        return LoadSurfaceView(exctx, gpu_addr, *cpu_addr, host_ptr, params, preserve_contents);
    }

    ResultType LoadSurfaceView(TExecutionContext exctx, GPUVAddr gpu_addr, VAddr cpu_addr,
                               u8* host_ptr, const SurfaceParams& params, bool preserve_contents) {
        const auto new_surface{GetUncachedSurface(params)};
        Register(new_surface, gpu_addr, cpu_addr, host_ptr);
        if (preserve_contents) {
            exctx = LoadSurface(exctx, new_surface);
        }
        return {new_surface->GetView(gpu_addr, params), exctx};
    }

    TExecutionContext LoadSurface(TExecutionContext exctx,
                                  const std::shared_ptr<TSurface>& surface) {
        surface->LoadBuffer();
        exctx = surface->UploadTexture(exctx);
        surface->MarkAsModified(false);
        return exctx;
    }

    TExecutionContext FlushSurface(TExecutionContext exctx,
                                   const std::shared_ptr<TSurface>& surface) {
        if (!surface->IsModified()) {
            return exctx;
        }
        exctx = surface->DownloadTexture(exctx);
        surface->FlushBuffer();
        return exctx;
    }

    std::vector<std::shared_ptr<TSurface>> GetSurfacesInRegion(CacheAddr cache_addr,
                                                               std::size_t size) const {
        if (size == 0) {
            return {};
        }
        const IntervalType interval{cache_addr, cache_addr + size};

        std::vector<std::shared_ptr<TSurface>> surfaces;
        for (auto& pair : boost::make_iterator_range(registered_surfaces.equal_range(interval))) {
            surfaces.push_back(*pair.second.begin());
        }
        return surfaces;
    }

    void ReserveSurface(const SurfaceParams& params, std::shared_ptr<TSurface> surface) {
        surface_reserve[params].push_back(std::move(surface));
    }

    std::shared_ptr<TSurface> TryGetReservedSurface(const SurfaceParams& params) {
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

    IntervalType GetSurfaceInterval(std::shared_ptr<TSurface> surface) const {
        return IntervalType::right_open(surface->GetCacheAddr(),
                                        surface->GetCacheAddr() + surface->GetSizeInBytes());
    }

    VideoCore::RasterizerInterface& rasterizer;

    u64 ticks{};

    IntervalMap registered_surfaces;

    /// The surface reserve is a "backup" cache, this is where we put unique surfaces that have
    /// previously been used. This is to prevent surfaces from being constantly created and
    /// destroyed when used with different surface parameters.
    std::unordered_map<SurfaceParams, std::list<std::shared_ptr<TSurface>>> surface_reserve;
};

} // namespace VideoCommon
