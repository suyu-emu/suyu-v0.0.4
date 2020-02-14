// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>

#include "common/common_types.h"
#include "video_core/query_cache.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace Core {
class System;
}

namespace OpenGL {

class CachedQuery;
class HostCounter;
class QueryCache;
class RasterizerOpenGL;

using CounterStream = VideoCommon::CounterStreamBase<QueryCache, HostCounter>;

class QueryCache final : public VideoCommon::QueryCacheBase<QueryCache, CachedQuery, CounterStream,
                                                            HostCounter, std::vector<OGLQuery>> {
public:
    explicit QueryCache(Core::System& system, RasterizerOpenGL& rasterizer);
    ~QueryCache();

    OGLQuery AllocateQuery(VideoCore::QueryType type);

    void Reserve(VideoCore::QueryType type, OGLQuery&& query);

    bool AnyCommandQueued() const noexcept;

private:
    RasterizerOpenGL& gl_rasterizer;
};

class HostCounter final : public VideoCommon::HostCounterBase<QueryCache, HostCounter> {
public:
    explicit HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency,
                         VideoCore::QueryType type);
    ~HostCounter();

    void EndQuery();

private:
    u64 BlockingQuery() const override;

    QueryCache& cache;
    const VideoCore::QueryType type;
    OGLQuery query;
};

class CachedQuery final : public VideoCommon::CachedQueryBase<HostCounter> {
public:
    explicit CachedQuery(QueryCache& cache, VideoCore::QueryType type, VAddr cpu_addr,
                         u8* host_ptr);
    CachedQuery(CachedQuery&& rhs) noexcept;
    CachedQuery(const CachedQuery&) = delete;

    CachedQuery& operator=(CachedQuery&& rhs) noexcept;
    CachedQuery& operator=(const CachedQuery&) = delete;

    void Flush() override;

private:
    QueryCache* cache;
    VideoCore::QueryType type;
};

} // namespace OpenGL
