// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/gpu.h"
#include "video_core/texture_cache/surface_params.h"
#include "video_core/texture_cache/surface_view.h"

namespace VideoCommon {

class SurfaceBaseImpl {
public:
    void LoadBuffer();

    void FlushBuffer();

    GPUVAddr GetGpuAddr() const {
        ASSERT(is_registered);
        return gpu_addr;
    }

    VAddr GetCpuAddr() const {
        ASSERT(is_registered);
        return cpu_addr;
    }

    u8* GetHostPtr() const {
        ASSERT(is_registered);
        return host_ptr;
    }

    CacheAddr GetCacheAddr() const {
        ASSERT(is_registered);
        return cache_addr;
    }

    const SurfaceParams& GetSurfaceParams() const {
        return params;
    }

    void Register(GPUVAddr gpu_addr_, VAddr cpu_addr_, u8* host_ptr_) {
        ASSERT(!is_registered);
        is_registered = true;
        gpu_addr = gpu_addr_;
        cpu_addr = cpu_addr_;
        host_ptr = host_ptr_;
        cache_addr = ToCacheAddr(host_ptr_);
        DecorateSurfaceName();
    }

    void Unregister() {
        ASSERT(is_registered);
        is_registered = false;
    }

    bool IsRegistered() const {
        return is_registered;
    }

    std::size_t GetSizeInBytes() const {
        return params.GetGuestSizeInBytes();
    }

    u8* GetStagingBufferLevelData(u32 level) {
        return staging_buffer.data() + params.GetHostMipmapLevelOffset(level);
    }

protected:
    explicit SurfaceBaseImpl(const SurfaceParams& params);
    ~SurfaceBaseImpl(); // non-virtual is intended

    virtual void DecorateSurfaceName() = 0;

    const SurfaceParams params;

private:
    GPUVAddr gpu_addr{};
    VAddr cpu_addr{};
    u8* host_ptr{};
    CacheAddr cache_addr{};
    bool is_registered{};

    std::vector<u8> staging_buffer;
};

template <typename TTextureCache, typename TView, typename TExecutionContext>
class SurfaceBase : public SurfaceBaseImpl {
    static_assert(std::is_trivially_copyable_v<TExecutionContext>);

public:
    virtual TExecutionContext UploadTexture(TExecutionContext exctx) = 0;

    virtual TExecutionContext DownloadTexture(TExecutionContext exctx) = 0;

    TView* TryGetView(GPUVAddr view_addr, const SurfaceParams& view_params) {
        if (view_addr < GetGpuAddr() || !params.IsFamiliar(view_params)) {
            // It can't be a view if it's in a prior address.
            return {};
        }

        const auto relative_offset{static_cast<u64>(view_addr - GetGpuAddr())};
        const auto it{view_offset_map.find(relative_offset)};
        if (it == view_offset_map.end()) {
            // Couldn't find an aligned view.
            return {};
        }
        const auto [layer, level] = it->second;

        if (!params.IsViewValid(view_params, layer, level)) {
            return {};
        }

        return GetView(layer, view_params.GetNumLayers(), level, view_params.GetNumLevels());
    }

    void MarkAsModified(bool is_modified_) {
        is_modified = is_modified_;
        if (is_modified_) {
            modification_tick = texture_cache.Tick();
        }
    }

    TView* GetView(GPUVAddr view_addr, const SurfaceParams& view_params) {
        TView* view{TryGetView(view_addr, view_params)};
        ASSERT(view != nullptr);
        return view;
    }

    bool IsModified() const {
        return is_modified;
    }

    u64 GetModificationTick() const {
        return modification_tick;
    }

protected:
    explicit SurfaceBase(TTextureCache& texture_cache, const SurfaceParams& params)
        : SurfaceBaseImpl{params}, texture_cache{texture_cache},
          view_offset_map{params.CreateViewOffsetMap()} {}

    ~SurfaceBase() = default;

    virtual std::unique_ptr<TView> CreateView(const ViewKey& view_key) = 0;

private:
    TView* GetView(u32 base_layer, u32 num_layers, u32 base_level, u32 num_levels) {
        const ViewKey key{base_layer, num_layers, base_level, num_levels};
        const auto [entry, is_cache_miss] = views.try_emplace(key);
        auto& view{entry->second};
        if (is_cache_miss) {
            view = CreateView(key);
        }
        return view.get();
    }

    TTextureCache& texture_cache;
    const std::map<u64, std::pair<u32, u32>> view_offset_map;

    std::unordered_map<ViewKey, std::unique_ptr<TView>> views;

    bool is_modified{};
    u64 modification_tick{};
};

} // namespace VideoCommon
