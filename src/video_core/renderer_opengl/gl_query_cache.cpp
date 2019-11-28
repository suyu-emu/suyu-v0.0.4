// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <unordered_map>
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

constexpr std::uintptr_t PAGE_SIZE = 4096;
constexpr int PAGE_SHIFT = 12;

constexpr std::size_t SMALL_QUERY_SIZE = 8;  // Query size without timestamp
constexpr std::size_t LARGE_QUERY_SIZE = 16; // Query size with timestamp
constexpr std::ptrdiff_t TIMESTAMP_OFFSET = 8;

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
        Enable();
    } else {
        Disable(any_command_queued);
    }
}

void CounterStream::Reset(bool any_command_queued) {
    if (current) {
        EndQuery(any_command_queued);

        // Immediately start a new query to avoid disabling its state.
        current = cache.GetHostCounter(nullptr, type);
    }
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

void CounterStream::Enable() {
    if (current) {
        return;
    }
    current = cache.GetHostCounter(last, type);
}

void CounterStream::Disable(bool any_command_queued) {
    if (current) {
        EndQuery(any_command_queued);
    }
    last = std::exchange(current, nullptr);
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
    : system{system}, rasterizer{rasterizer}, streams{{CounterStream{*this,
                                                                     QueryType::SamplesPassed}}} {}

QueryCache::~QueryCache() = default;

void QueryCache::InvalidateRegion(CacheAddr addr, std::size_t size) {
    const u64 addr_begin = static_cast<u64>(addr);
    const u64 addr_end = addr_begin + static_cast<u64>(size);
    const auto in_range = [addr_begin, addr_end](CachedQuery& query) {
        const u64 cache_begin = query.GetCacheAddr();
        const u64 cache_end = cache_begin + query.GetSizeInBytes();
        return cache_begin < addr_end && addr_begin < cache_end;
    };

    const u64 page_end = addr_end >> PAGE_SHIFT;
    for (u64 page = addr_begin >> PAGE_SHIFT; page <= page_end; ++page) {
        const auto& it = cached_queries.find(page);
        if (it == std::end(cached_queries)) {
            continue;
        }
        auto& contents = it->second;
        for (auto& query : contents) {
            if (!in_range(query)) {
                continue;
            }
            rasterizer.UpdatePagesCachedCount(query.GetCpuAddr(), query.GetSizeInBytes(), -1);
            Flush(query);
        }
        contents.erase(std::remove_if(std::begin(contents), std::end(contents), in_range),
                       std::end(contents));
    }
}

void QueryCache::FlushRegion(CacheAddr addr, std::size_t size) {
    // We can handle flushes in the same way as invalidations.
    InvalidateRegion(addr, size);
}

void QueryCache::Query(GPUVAddr gpu_addr, QueryType type, std::optional<u64> timestamp) {
    auto& memory_manager = system.GPU().MemoryManager();
    const auto host_ptr = memory_manager.GetPointer(gpu_addr);

    CachedQuery* query = TryGet(ToCacheAddr(host_ptr));
    if (!query) {
        const auto cpu_addr = memory_manager.GpuToCpuAddress(gpu_addr);
        ASSERT_OR_EXECUTE(cpu_addr, return;);

        query = &Register(CachedQuery(type, *cpu_addr, host_ptr));
    }

    query->SetCounter(GetStream(type).GetCurrent(rasterizer.AnyCommandQueued()), timestamp);
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
    auto& reserve = reserved_queries[static_cast<std::size_t>(type)];
    OGLQuery query;
    if (reserve.empty()) {
        query.Create(GetTarget(type));
    } else {
        query = std::move(reserve.back());
        reserve.pop_back();
    }

    return std::make_shared<HostCounter>(*this, std::move(dependency), type, std::move(query));
}

CachedQuery& QueryCache::Register(CachedQuery&& cached_query) {
    const u64 page = static_cast<u64>(cached_query.GetCacheAddr()) >> PAGE_SHIFT;
    auto& stored_ref = cached_queries[page].emplace_back(std::move(cached_query));
    rasterizer.UpdatePagesCachedCount(stored_ref.GetCpuAddr(), stored_ref.GetSizeInBytes(), 1);
    return stored_ref;
}

CachedQuery* QueryCache::TryGet(CacheAddr addr) {
    const u64 page = static_cast<u64>(addr) >> PAGE_SHIFT;
    const auto it = cached_queries.find(page);
    if (it == std::end(cached_queries)) {
        return nullptr;
    }
    auto& contents = it->second;
    const auto found =
        std::find_if(std::begin(contents), std::end(contents),
                     [addr](const auto& query) { return query.GetCacheAddr() == addr; });
    return found != std::end(contents) ? &*found : nullptr;
}

void QueryCache::Flush(CachedQuery& cached_query) {
    auto& stream = GetStream(cached_query.GetType());

    // Waiting for a query while another query of the same target is enabled locks Nvidia's driver.
    // To avoid this disable and re-enable keeping the dependency stream.
    // But we only have to do this if we have pending waits to be done.
    const bool slice_counter = stream.IsEnabled() && cached_query.WaitPending();
    const bool any_command_queued = rasterizer.AnyCommandQueued();
    if (slice_counter) {
        stream.Update(false, any_command_queued);
    }

    cached_query.Flush();

    if (slice_counter) {
        stream.Update(true, any_command_queued);
    }
}

CounterStream& QueryCache::GetStream(QueryType type) {
    return streams[static_cast<std::size_t>(type)];
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
    if (result) {
        return *result;
    }

    u64 value;
    glGetQueryObjectui64v(query.handle, GL_QUERY_RESULT, &value);
    if (dependency) {
        value += dependency->Query();
    }

    return *(result = value);
}

bool HostCounter::WaitPending() const noexcept {
    return result.has_value();
}

CachedQuery::CachedQuery(QueryType type, VAddr cpu_addr, u8* host_ptr)
    : type{type}, cpu_addr{cpu_addr}, host_ptr{host_ptr} {}

CachedQuery::CachedQuery(CachedQuery&& rhs) noexcept
    : type{rhs.type}, cpu_addr{rhs.cpu_addr}, host_ptr{rhs.host_ptr},
      counter{std::move(rhs.counter)}, timestamp{rhs.timestamp} {}

CachedQuery::~CachedQuery() = default;

CachedQuery& CachedQuery::operator=(CachedQuery&& rhs) noexcept {
    type = rhs.type;
    cpu_addr = rhs.cpu_addr;
    host_ptr = rhs.host_ptr;
    counter = std::move(rhs.counter);
    timestamp = rhs.timestamp;
    return *this;
}

void CachedQuery::Flush() {
    // When counter is nullptr it means that it's just been reseted. We are supposed to write a zero
    // in these cases.
    const u64 value = counter ? counter->Query() : 0;
    std::memcpy(host_ptr, &value, sizeof(u64));

    if (timestamp) {
        std::memcpy(host_ptr + TIMESTAMP_OFFSET, &*timestamp, sizeof(u64));
    }
}

void CachedQuery::SetCounter(std::shared_ptr<HostCounter> counter_, std::optional<u64> timestamp_) {
    if (counter) {
        // If there's an old counter set it means the query is being rewritten by the game.
        // To avoid losing the data forever, flush here.
        Flush();
    }
    counter = std::move(counter_);
    timestamp = timestamp_;
}

bool CachedQuery::WaitPending() const noexcept {
    return counter && counter->WaitPending();
}

QueryType CachedQuery::GetType() const noexcept {
    return type;
}

VAddr CachedQuery::GetCpuAddr() const noexcept {
    return cpu_addr;
}

CacheAddr CachedQuery::GetCacheAddr() const noexcept {
    return ToCacheAddr(host_ptr);
}

u64 CachedQuery::GetSizeInBytes() const noexcept {
    return timestamp ? LARGE_QUERY_SIZE : SMALL_QUERY_SIZE;
}

} // namespace OpenGL
