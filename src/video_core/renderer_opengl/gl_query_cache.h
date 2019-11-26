// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <vector>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/rasterizer_cache.h"
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
    void EndQuery(bool any_command_queued);

    QueryCache& cache;

    std::shared_ptr<HostCounter> current;
    std::shared_ptr<HostCounter> last;
    VideoCore::QueryType type;
    GLenum target;
};

class QueryCache final : public RasterizerCache<std::shared_ptr<CachedQuery>> {
public:
    explicit QueryCache(Core::System& system, RasterizerOpenGL& rasterizer);
    ~QueryCache();

    void Query(GPUVAddr gpu_addr, VideoCore::QueryType type);

    void UpdateCounters();

    void ResetCounter(VideoCore::QueryType type);

    void Reserve(VideoCore::QueryType type, OGLQuery&& query);

    std::shared_ptr<HostCounter> GetHostCounter(std::shared_ptr<HostCounter> dependency,
                                                VideoCore::QueryType type);

protected:
    void FlushObjectInner(const std::shared_ptr<CachedQuery>& counter) override;

private:
    CounterStream& GetStream(VideoCore::QueryType type);

    Core::System& system;
    RasterizerOpenGL& rasterizer;

    std::array<CounterStream, VideoCore::NumQueryTypes> streams;
    std::array<std::vector<OGLQuery>, VideoCore::NumQueryTypes> reserved_queries;
};

class HostCounter final {
public:
    explicit HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type);
    explicit HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type, OGLQuery&& query);
    ~HostCounter();

    /// Returns the current value of the query.
    u64 Query();

private:
    QueryCache& cache;
    VideoCore::QueryType type;

    std::shared_ptr<HostCounter> dependency; ///< Counter queued before this one.
    OGLQuery query;                          ///< OpenGL query.
    u64 result;                              ///< Added values of the counter.
};

class CachedQuery final : public RasterizerCacheObject {
public:
    explicit CachedQuery(VideoCore::QueryType type, VAddr cpu_addr, u8* host_ptr);
    ~CachedQuery();

    /// Writes the counter value to host memory.
    void Flush();

    /// Updates the counter this cached query registered in guest memory will write when requested.
    void SetCounter(std::shared_ptr<HostCounter> counter);

    /// Returns the query type.
    VideoCore::QueryType GetType() const;

    VAddr GetCpuAddr() const override {
        return cpu_addr;
    }

    std::size_t GetSizeInBytes() const override {
        return sizeof(u64);
    }

private:
    VideoCore::QueryType type;
    VAddr cpu_addr;                       ///< Guest CPU address.
    u8* host_ptr;                         ///< Writable host pointer.
    std::shared_ptr<HostCounter> counter; ///< Host counter to query, owns the dependency tree.
};

} // namespace OpenGL
