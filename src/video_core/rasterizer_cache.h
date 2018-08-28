// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>

#include "common/common_types.h"
#include "core/core.h"
#include "core/memory.h"
#include "video_core/memory_manager.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_base.h"

template <class T>
class RasterizerCache : NonCopyable {
public:
    /// Mark the specified region as being invalidated
    void InvalidateRegion(Tegra::GPUVAddr region_addr, size_t region_size) {
        for (auto iter = cached_objects.cbegin(); iter != cached_objects.cend();) {
            const auto& object{iter->second};

            ++iter;

            if (object->GetAddr() <= (region_addr + region_size) &&
                region_addr <= (object->GetAddr() + object->GetSizeInBytes())) {
                // Regions overlap, so invalidate
                Unregister(object);
            }
        }
    }

protected:
    /// Tries to get an object from the cache with the specified address
    T TryGet(Tegra::GPUVAddr addr) const {
        const auto& search{cached_objects.find(addr)};
        if (search != cached_objects.end()) {
            return search->second;
        }

        return nullptr;
    }

    /// Gets a reference to the cache
    const std::unordered_map<Tegra::GPUVAddr, T>& GetCache() const {
        return cached_objects;
    }

    /// Register an object into the cache
    void Register(const T& object) {
        const auto& search{cached_objects.find(object->GetAddr())};
        if (search != cached_objects.end()) {
            // Registered already
            return;
        }

        auto& rasterizer = Core::System::GetInstance().Renderer().Rasterizer();
        rasterizer.UpdatePagesCachedCount(object->GetAddr(), object->GetSizeInBytes(), 1);
        cached_objects[object->GetAddr()] = std::move(object);
    }

    /// Unregisters an object from the cache
    void Unregister(const T& object) {
        const auto& search{cached_objects.find(object->GetAddr())};
        if (search == cached_objects.end()) {
            // Unregistered already
            return;
        }

        auto& rasterizer = Core::System::GetInstance().Renderer().Rasterizer();
        rasterizer.UpdatePagesCachedCount(object->GetAddr(), object->GetSizeInBytes(), -1);
        cached_objects.erase(search);
    }

private:
    std::unordered_map<Tegra::GPUVAddr, T> cached_objects;
};
