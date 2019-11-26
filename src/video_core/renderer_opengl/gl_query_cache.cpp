// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "common/assert.h"
#include "core/core.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_query_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"

namespace OpenGL {

using VideoCore::QueryType;

namespace {

constexpr std::array<GLenum, VideoCore::NumQueryTypes> QueryTargets = {GL_SAMPLES_PASSED};

constexpr GLenum GetTarget(QueryType type) {
    return QueryTargets[static_cast<std::size_t>(type)];
}

} // Anonymous namespace

CounterStream::CounterStream(QueryCache& cache, QueryType type)
    : cache{cache}, type{type}, target{GetTarget(type)} {}

CounterStream::~CounterStream() = default;

void CounterStream::Update(bool enabled, bool any_command_queued) {
    if (enabled) {
        if (!current) {
            current = cache.GetHostCounter(last, type);
        }
        return;
    }

    if (current) {
        EndQuery(any_command_queued);
    }
    last = std::exchange(current, nullptr);
}

void CounterStream::Reset(bool any_command_queued) {
    if (current) {
        EndQuery(any_command_queued);
    }
    current = nullptr;
    last = nullptr;
}

std::shared_ptr<HostCounter> CounterStream::GetCurrent(bool any_command_queued) {
    if (!current) {
        return nullptr;
    }
    EndQuery(any_command_queued);
    last = std::move(current);
    current = cache.GetHostCounter(last, type);
    return last;
}

void CounterStream::EndQuery(bool any_command_queued) {
    if (!any_command_queued) {
        // There are chances a query waited on without commands (glDraw, glClear, glDispatch). Not
        // having any of these causes a lock. glFlush is considered a command, so we can safely wait
        // for this. Insert to the OpenGL command stream a flush.
        glFlush();
    }
    glEndQuery(target);
}

QueryCache::QueryCache(Core::System& system, RasterizerOpenGL& rasterizer)
    : RasterizerCache{rasterizer}, system{system},
      rasterizer{rasterizer}, streams{{CounterStream{*this, QueryType::SamplesPassed}}} {}

QueryCache::~QueryCache() = default;

void QueryCache::Query(GPUVAddr gpu_addr, QueryType type) {
    auto& memory_manager = system.GPU().MemoryManager();
    const auto host_ptr = memory_manager.GetPointer(gpu_addr);

    auto query = TryGet(host_ptr);
    if (!query) {
        const auto cpu_addr = memory_manager.GpuToCpuAddress(gpu_addr);
        ASSERT_OR_EXECUTE(cpu_addr, return;);

        query = std::make_shared<CachedQuery>(type, *cpu_addr, host_ptr);
        Register(query);
    }

    query->SetCounter(GetStream(type).GetCurrent(rasterizer.AnyCommandQueued()));
    query->MarkAsModified(true, *this);
}

void QueryCache::UpdateCounters() {
    auto& samples_passed = GetStream(QueryType::SamplesPassed);

    const auto& regs = system.GPU().Maxwell3D().regs;
    samples_passed.Update(regs.samplecnt_enable, rasterizer.AnyCommandQueued());
}

void QueryCache::ResetCounter(QueryType type) {
    GetStream(type).Reset(rasterizer.AnyCommandQueued());
}

void QueryCache::Reserve(QueryType type, OGLQuery&& query) {
    reserved_queries[static_cast<std::size_t>(type)].push_back(std::move(query));
}

std::shared_ptr<HostCounter> QueryCache::GetHostCounter(std::shared_ptr<HostCounter> dependency,
                                                        QueryType type) {
    const auto type_index = static_cast<std::size_t>(type);
    auto& reserve = reserved_queries[type_index];

    if (reserve.empty()) {
        return std::make_shared<HostCounter>(*this, std::move(dependency), type);
    }

    auto counter = std::make_shared<HostCounter>(*this, std::move(dependency), type,
                                                 std::move(reserve.back()));
    reserve.pop_back();
    return counter;
}

void QueryCache::FlushObjectInner(const std::shared_ptr<CachedQuery>& counter_) {
    auto& counter = *counter_;
    auto& stream = GetStream(counter.GetType());

    // Waiting for a query while another query of the same target is enabled locks Nvidia's driver.
    // To avoid this disable and re-enable keeping the dependency stream.
    const bool is_enabled = stream.IsEnabled();
    if (is_enabled) {
        stream.Update(false, false);
    }

    counter.Flush();

    if (is_enabled) {
        stream.Update(true, false);
    }
}

CounterStream& QueryCache::GetStream(QueryType type) {
    return streams[static_cast<std::size_t>(type)];
}

HostCounter::HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency, QueryType type)
    : cache{cache}, type{type}, dependency{std::move(dependency)} {
    const GLenum target = GetTarget(type);
    query.Create(target);
    glBeginQuery(target, query.handle);
}

HostCounter::HostCounter(QueryCache& cache, std::shared_ptr<HostCounter> dependency, QueryType type,
                         OGLQuery&& query_)
    : cache{cache}, type{type}, dependency{std::move(dependency)}, query{std::move(query_)} {
    glBeginQuery(GetTarget(type), query.handle);
}

HostCounter::~HostCounter() {
    cache.Reserve(type, std::move(query));
}

u64 HostCounter::Query() {
    if (query.handle == 0) {
        return result;
    }

    glGetQueryObjectui64v(query.handle, GL_QUERY_RESULT, &result);

    if (dependency) {
        result += dependency->Query();
    }

    return result;
}

CachedQuery::CachedQuery(QueryType type, VAddr cpu_addr, u8* host_ptr)
    : RasterizerCacheObject{host_ptr}, type{type}, cpu_addr{cpu_addr}, host_ptr{host_ptr} {}

CachedQuery::~CachedQuery() = default;

void CachedQuery::Flush() {
    const u64 value = counter->Query();
    std::memcpy(host_ptr, &value, sizeof(value));
}

void CachedQuery::SetCounter(std::shared_ptr<HostCounter> counter_) {
    counter = std::move(counter_);
}

QueryType CachedQuery::GetType() const {
    return type;
}

} // namespace OpenGL
