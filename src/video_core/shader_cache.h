// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"

namespace VideoCommon {

template <class T>
class ShaderCache {
    static constexpr u64 PAGE_BITS = 14;

    struct Entry {
        VAddr addr_start;
        VAddr addr_end;
        T* data;

        bool is_memory_marked = true;

        constexpr bool Overlaps(VAddr start, VAddr end) const noexcept {
            return start < addr_end && addr_start < end;
        }
    };

public:
    virtual ~ShaderCache() = default;

    /// @brief Removes shaders inside a given region
    /// @note Checks for ranges
    /// @param addr Start address of the invalidation
    /// @param size Number of bytes of the invalidation
    void InvalidateRegion(VAddr addr, std::size_t size) {
        std::scoped_lock lock{invalidation_mutex};
        InvalidatePagesInRegion(addr, size);
        RemovePendingShaders();
    }

    /// @brief Unmarks a memory region as cached and marks it for removal
    /// @param addr Start address of the CPU write operation
    /// @param size Number of bytes of the CPU write operation
    void OnCPUWrite(VAddr addr, std::size_t size) {
        std::lock_guard lock{invalidation_mutex};
        InvalidatePagesInRegion(addr, size);
    }

    /// @brief Flushes delayed removal operations
    void SyncGuestHost() {
        std::scoped_lock lock{invalidation_mutex};
        RemovePendingShaders();
    }

    /// @brief Tries to obtain a cached shader starting in a given address
    /// @note Doesn't check for ranges, the given address has to be the start of the shader
    /// @param addr Start address of the shader, this doesn't cache for region
    /// @return Pointer to a valid shader, nullptr when nothing is found
    T* TryGet(VAddr addr) const {
        std::scoped_lock lock{lookup_mutex};

        const auto it = lookup_cache.find(addr);
        if (it == lookup_cache.end()) {
            return nullptr;
        }
        return it->second->data;
    }

protected:
    explicit ShaderCache(VideoCore::RasterizerInterface& rasterizer_) : rasterizer{rasterizer_} {}

    /// @brief Register in the cache a given entry
    /// @param data Shader to store in the cache
    /// @param addr Start address of the shader that will be registered
    /// @param size Size in bytes of the shader
    void Register(std::unique_ptr<T> data, VAddr addr, std::size_t size) {
        std::scoped_lock lock{invalidation_mutex, lookup_mutex};

        const VAddr addr_end = addr + size;
        Entry* const entry = NewEntry(addr, addr_end, data.get());

        const u64 page_end = addr_end >> PAGE_BITS;
        for (u64 page = addr >> PAGE_BITS; page <= page_end; ++page) {
            invalidation_cache[page].push_back(entry);
        }

        storage.push_back(std::move(data));

        rasterizer.UpdatePagesCachedCount(addr, size, 1);
    }

    /// @brief Called when a shader is going to be removed
    /// @param shader Shader that will be removed
    /// @pre invalidation_cache is locked
    /// @pre lookup_mutex is locked
    virtual void OnShaderRemoval([[maybe_unused]] T* shader) {}

private:
    /// @brief Invalidate pages in a given region
    /// @pre invalidation_mutex is locked
    void InvalidatePagesInRegion(VAddr addr, std::size_t size) {
        const VAddr addr_end = addr + size;
        const u64 page_end = addr_end >> PAGE_BITS;
        for (u64 page = addr >> PAGE_BITS; page <= page_end; ++page) {
            const auto it = invalidation_cache.find(page);
            if (it == invalidation_cache.end()) {
                continue;
            }

            std::vector<Entry*>& entries = it->second;
            InvalidatePageEntries(entries, addr, addr_end);

            // If there's nothing else in this page, remove it to avoid overpopulating the hash map.
            if (entries.empty()) {
                invalidation_cache.erase(it);
            }
        }
    }

    /// @brief Remove shaders marked for deletion
    /// @pre invalidation_mutex is locked
    void RemovePendingShaders() {
        if (marked_for_removal.empty()) {
            return;
        }
        std::scoped_lock lock{lookup_mutex};

        std::vector<T*> removed_shaders;
        removed_shaders.reserve(marked_for_removal.size());

        for (Entry* const entry : marked_for_removal) {
            if (lookup_cache.erase(entry->addr_start) > 0) {
                removed_shaders.push_back(entry->data);
            }
        }
        marked_for_removal.clear();

        if (!removed_shaders.empty()) {
            RemoveShadersFromStorage(std::move(removed_shaders));
        }
    }

    /// @brief Invalidates entries in a given range for the passed page
    /// @param entries         Vector of entries in the page, it will be modified on overlaps
    /// @param addr            Start address of the invalidation
    /// @param addr_end        Non-inclusive end address of the invalidation
    /// @pre invalidation_mutex is locked
    void InvalidatePageEntries(std::vector<Entry*>& entries, VAddr addr, VAddr addr_end) {
        auto it = entries.begin();
        while (it != entries.end()) {
            Entry* const entry = *it;
            if (!entry->Overlaps(addr, addr_end)) {
                ++it;
                continue;
            }
            UnmarkMemory(entry);
            marked_for_removal.push_back(entry);

            it = entries.erase(it);
        }
    }

    /// @brief Unmarks an entry from the rasterizer cache
    /// @param entry Entry to unmark from memory
    void UnmarkMemory(Entry* entry) {
        if (!entry->is_memory_marked) {
            return;
        }
        entry->is_memory_marked = false;

        const VAddr addr = entry->addr_start;
        const std::size_t size = entry->addr_end - addr;
        rasterizer.UpdatePagesCachedCount(addr, size, -1);
    }

    /// @brief Removes a vector of shaders from a list
    /// @param removed_shaders Shaders to be removed from the storage, it can contain duplicates
    /// @pre invalidation_mutex is locked
    /// @pre lookup_mutex is locked
    void RemoveShadersFromStorage(std::vector<T*> removed_shaders) {
        // Remove duplicates
        std::sort(removed_shaders.begin(), removed_shaders.end());
        removed_shaders.erase(std::unique(removed_shaders.begin(), removed_shaders.end()),
                              removed_shaders.end());

        // Now that there are no duplicates, we can notify removals
        for (T* const shader : removed_shaders) {
            OnShaderRemoval(shader);
        }

        // Remove them from the cache
        const auto is_removed = [&removed_shaders](std::unique_ptr<T>& shader) {
            return std::find(removed_shaders.begin(), removed_shaders.end(), shader.get()) !=
                   removed_shaders.end();
        };
        storage.erase(std::remove_if(storage.begin(), storage.end(), is_removed), storage.end());
    }

    /// @brief Creates a new entry in the lookup cache and returns its pointer
    /// @pre lookup_mutex is locked
    Entry* NewEntry(VAddr addr, VAddr addr_end, T* data) {
        auto entry = std::make_unique<Entry>(Entry{addr, addr_end, data});
        Entry* const entry_pointer = entry.get();

        lookup_cache.emplace(addr, std::move(entry));
        return entry_pointer;
    }

    VideoCore::RasterizerInterface& rasterizer;

    mutable std::mutex lookup_mutex;
    std::mutex invalidation_mutex;

    std::unordered_map<u64, std::unique_ptr<Entry>> lookup_cache;
    std::unordered_map<u64, std::vector<Entry*>> invalidation_cache;
    std::vector<std::unique_ptr<T>> storage;
    std::vector<Entry*> marked_for_removal;
};

} // namespace VideoCommon
