// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "video_core/texture_cache/texture_cache.h"

namespace VideoCommon {

struct DummyExecutionContext {};

template <typename TSurface, typename TView>
class TextureCacheContextless : protected TextureCache<TSurface, TView, DummyExecutionContext> {
    using Base = TextureCache<TSurface, TView, DummyExecutionContext>;

public:
    void InvalidateRegion(CacheAddr addr, std::size_t size) {
        Base::InvalidateRegion(addr, size);
    }

    TView* GetTextureSurface(const Tegra::Texture::FullTextureInfo& config) {
        return RemoveContext(Base::GetTextureSurface({}, config));
    }

    TView* GetDepthBufferSurface(bool preserve_contents) {
        return RemoveContext(Base::GetDepthBufferSurface({}, preserve_contents));
    }

    TView* GetColorBufferSurface(std::size_t index, bool preserve_contents) {
        return RemoveContext(Base::GetColorBufferSurface({}, index, preserve_contents));
    }

    TView* GetFermiSurface(const Tegra::Engines::Fermi2D::Regs::Surface& config) {
        return RemoveContext(Base::GetFermiSurface({}, config));
    }

    std::shared_ptr<TSurface> TryFindFramebufferSurface(const u8* host_ptr) const {
        return Base::TryFindFramebufferSurface(host_ptr);
    }

    u64 Tick() {
        return Base::Tick();
    }

protected:
    explicit TextureCacheContextless(Core::System& system,
                                     VideoCore::RasterizerInterface& rasterizer)
        : TextureCache<TSurface, TView, DummyExecutionContext>{system, rasterizer} {}

    virtual TView* TryFastGetSurfaceView(
        GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr, const SurfaceParams& params,
        bool preserve_contents, const std::vector<std::shared_ptr<TSurface>>& overlaps) = 0;

private:
    std::tuple<TView*, DummyExecutionContext> TryFastGetSurfaceView(
        DummyExecutionContext, GPUVAddr gpu_addr, VAddr cpu_addr, u8* host_ptr,
        const SurfaceParams& params, bool preserve_contents,
        const std::vector<std::shared_ptr<TSurface>>& overlaps) {
        return {TryFastGetSurfaceView(gpu_addr, cpu_addr, host_ptr, params, preserve_contents,
                                      overlaps),
                {}};
    }

    TView* RemoveContext(std::tuple<TView*, DummyExecutionContext> return_value) {
        const auto [view, exctx] = return_value;
        return view;
    }
};

template <typename TTextureCache, typename TView>
class SurfaceBaseContextless : public SurfaceBase<TTextureCache, TView, DummyExecutionContext> {
public:
    DummyExecutionContext DownloadTexture(DummyExecutionContext) {
        DownloadTextureImpl();
        return {};
    }

    DummyExecutionContext UploadTexture(DummyExecutionContext) {
        UploadTextureImpl();
        return {};
    }

protected:
    explicit SurfaceBaseContextless(TTextureCache& texture_cache, const SurfaceParams& params)
        : SurfaceBase<TTextureCache, TView, DummyExecutionContext>{texture_cache, params} {}

    virtual void DownloadTextureImpl() = 0;

    virtual void UploadTextureImpl() = 0;
};

} // namespace VideoCommon
