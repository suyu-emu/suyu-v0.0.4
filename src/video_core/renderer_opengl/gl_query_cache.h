// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Core {
class System;
}

namespace OpenGL {

class CachedQuery;
class HostCounter;
class RasterizerOpenGL;
class QueryCache;

class CounterStream final {
public:
    explicit CounterStream(QueryCache& cache, VideoCore::QueryType type);
    ~CounterStream();

    void Update(bool enabled, bool any_command_queued);

    void Reset(bool any_command_queued);

    std::shared_ptr<HostCounter> GetCurrent(bool any_command_queued);

    bool IsEnabled() const {
        return current != nullptr;
    }

private:
    void Enable();

    void Disable(bool any_command_queued);

    void EndQuery(bool any_command_queued);

    QueryCache& cache;

    std::shared_ptr<HostCounter> current;
    std::shared_ptr<HostCounter> last;
    VideoCore::QueryType type;
    GLenum target;
};

class QueryCache final {
public:
    explicit QueryCache(Core::System& system, RasterizerOpenGL& rasterizer);
    ~QueryCache();

    void InvalidateRegion(CacheAddr addr, std::size_t size);

    void FlushRegion(CacheAddr addr, std::size_t size);

    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type, std::optional<u64> timestamp);

    void UpdateCounters();

    void ResetCounter(VideoCore::QueryType type);

    void Reserve(VideoCore::QueryType type, OGLQuery&& query);

    std::shared_ptr<HostCounter> GetHostCounter(std::shared_ptr<HostCounter> dependency,
                                                VideoCore::QueryType type);

private:
    CachedQuery& Register(CachedQuery&& cached_query);

    CachedQuery* TryGet(CacheAddr addr);

    void Flush(CachedQuery& cached_query);

    CounterStream& GetStream(VideoCore::QueryType type);

    Core::System& system;
    RasterizerOpenGL& rasterizer;

    std::unordered_map<u64, std::vector<CachedQuery>> cached_queries;

    std::array<CounterStream, VideoCore::NumQueryTypes> streams;
    std::array<std::vector<OGLQuery>, VideoCore::NumQueryTypes> reserved_queries;
};

class HostCounter final {
public:
    explicit HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type, OGLQuery&& query);
    ~HostCounter();

    /// Returns the current value of the query.
    u64 Query();

    /// Returns true when querying this counter will potentially wait for OpenGL.
    bool WaitPending() const noexcept;

private:
    QueryCache& cache;
    VideoCore::QueryType type;

    std::shared_ptr<HostCounter> dependency; ///< Counter queued before this one.
    OGLQuery query;                          ///< OpenGL query.
    std::optional<u64> result;               ///< Added values of the counter.
};

class CachedQuery final {
public:
    explicit CachedQuery(VideoCore::QueryType type, VAddr cpu_addr, u8* host_ptr);
    CachedQuery(CachedQuery&&) noexcept;
    CachedQuery(const CachedQuery&) = delete;
    ~CachedQuery();

    CachedQuery& operator=(CachedQuery&&) noexcept;

    /// Writes the counter value to host memory.
    void Flush();

    /// Updates the counter this cached query registered in guest memory will write when requested.
    void SetCounter(std::shared_ptr<HostCounter> counter, std::optional<u64> timestamp);

    /// Returns true when a flushing this query will potentially wait for OpenGL.
    bool WaitPending() const noexcept;

    /// Returns the query type.
    VideoCore::QueryType GetType() const noexcept;

    /// Returns the guest CPU address for this query.
    VAddr GetCpuAddr() const noexcept;

    /// Returns the cache address for this query.
    CacheAddr GetCacheAddr() const noexcept;

    /// Returns the number of cached bytes.
    u64 GetSizeInBytes() const noexcept;

private:
    VideoCore::QueryType type;            ///< Abstracted query type (e.g. samples passed).
    VAddr cpu_addr;                       ///< Guest CPU address.
    u8* host_ptr;                         ///< Writable host pointer.
    std::shared_ptr<HostCounter> counter; ///< Host counter to query, owns the dependency tree.
    std::optional<u64> timestamp;         ///< Timestamp to flush to guest memory.
};

} // namespace OpenGL
