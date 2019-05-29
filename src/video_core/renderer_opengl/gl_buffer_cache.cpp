// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <utility>

#include "common/alignment.h"
#include "common/assert.h"
#include "core/core.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_opengl/gl_buffer_cache.h"
#include "video_core/renderer_opengl/gl_rasterizer.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

namespace {

constexpr GLuint EmptyBuffer = 0;
constexpr GLintptr CachedBufferOffset = 0;

OGLBuffer CreateBuffer(std::size_t size, GLenum usage) {
    OGLBuffer buffer;
    buffer.Create();
    glNamedBufferData(buffer.handle, size, nullptr, usage);
    return buffer;
}

} // Anonymous namespace

CachedBufferEntry::CachedBufferEntry(VAddr cpu_addr, u8* host_ptr)
    : RasterizerCacheObject{host_ptr}, cpu_addr{cpu_addr} {}

OGLBufferCache::OGLBufferCache(RasterizerOpenGL& rasterizer, std::size_t size)
    : RasterizerCache{rasterizer}, stream_buffer(size, true) {}

OGLBufferCache::~OGLBufferCache() = default;

void OGLBufferCache::Unregister(const std::shared_ptr<CachedBufferEntry>& entry) {
    std::lock_guard lock{mutex};

    if (entry->IsInternalized()) {
        internalized_entries.erase(entry->GetCacheAddr());
    }
    ReserveBuffer(entry);
    RasterizerCache<std::shared_ptr<CachedBufferEntry>>::Unregister(entry);
}

OGLBufferCache::BufferInfo OGLBufferCache::UploadMemory(GPUVAddr gpu_addr, std::size_t size,
                                                        std::size_t alignment, bool internalize,
                                                        bool is_written) {
    std::lock_guard lock{mutex};

    auto& memory_manager = Core::System::GetInstance().GPU().MemoryManager();
    const auto host_ptr{memory_manager.GetPointer(gpu_addr)};
    const auto cache_addr{ToCacheAddr(host_ptr)};
    if (!host_ptr) {
        return {EmptyBuffer, 0};
    }

    // Cache management is a big overhead, so only cache entries with a given size.
    // TODO: Figure out which size is the best for given games.
    if (!internalize && size < 0x800 &&
        internalized_entries.find(cache_addr) == internalized_entries.end()) {
        return StreamBufferUpload(host_ptr, size, alignment);
    }

    auto entry = TryGet(host_ptr);
    if (!entry) {
        return FixedBufferUpload(gpu_addr, host_ptr, size, internalize, is_written);
    }

    if (entry->GetSize() < size) {
        GrowBuffer(entry, size);
    }
    if (is_written) {
        entry->MarkAsModified(true, *this);
    }
    return {entry->GetBuffer(), CachedBufferOffset};
}

OGLBufferCache::BufferInfo OGLBufferCache::UploadHostMemory(const void* raw_pointer,
                                                            std::size_t size,
                                                            std::size_t alignment) {
    std::lock_guard lock{mutex};
    return StreamBufferUpload(raw_pointer, size, alignment);
}

bool OGLBufferCache::Map(std::size_t max_size) {
    const auto max_size_ = static_cast<GLsizeiptr>(max_size);
    bool invalidate;
    std::tie(buffer_ptr, buffer_offset_base, invalidate) = stream_buffer.Map(max_size_, 4);
    buffer_offset = buffer_offset_base;
    return invalidate;
}

void OGLBufferCache::Unmap() {
    stream_buffer.Unmap(buffer_offset - buffer_offset_base);
}

OGLBufferCache::BufferInfo OGLBufferCache::StreamBufferUpload(const void* raw_pointer,
                                                              std::size_t size,
                                                              std::size_t alignment) {
    AlignBuffer(alignment);
    const GLintptr uploaded_offset = buffer_offset;
    std::memcpy(buffer_ptr, raw_pointer, size);

    buffer_ptr += size;
    buffer_offset += size;
    return {stream_buffer.GetHandle(), uploaded_offset};
}

OGLBufferCache::BufferInfo OGLBufferCache::FixedBufferUpload(GPUVAddr gpu_addr, u8* host_ptr,
                                                             std::size_t size, bool internalize,
                                                             bool is_written) {
    auto& memory_manager = Core::System::GetInstance().GPU().MemoryManager();
    const auto cpu_addr = *memory_manager.GpuToCpuAddress(gpu_addr);
    auto entry = GetUncachedBuffer(cpu_addr, host_ptr);
    entry->SetSize(size);
    entry->SetInternalState(internalize);
    Register(entry);

    if (internalize) {
        internalized_entries.emplace(ToCacheAddr(host_ptr));
    }
    if (is_written) {
        entry->MarkAsModified(true, *this);
    }

    if (entry->GetCapacity() < size) {
        entry->SetCapacity(CreateBuffer(size, GL_STATIC_DRAW), size);
    }
    glNamedBufferSubData(entry->GetBuffer(), 0, static_cast<GLintptr>(size), host_ptr);
    return {entry->GetBuffer(), CachedBufferOffset};
}

void OGLBufferCache::GrowBuffer(std::shared_ptr<CachedBufferEntry>& entry, std::size_t new_size) {
    const auto old_size = static_cast<GLintptr>(entry->GetSize());
    if (entry->GetCapacity() < new_size) {
        const auto old_buffer = entry->GetBuffer();
        OGLBuffer new_buffer = CreateBuffer(new_size, GL_STATIC_COPY);

        // Copy bits from the old buffer to the new buffer.
        glCopyNamedBufferSubData(old_buffer, new_buffer.handle, 0, 0, old_size);
        entry->SetCapacity(std::move(new_buffer), new_size);
    }
    // Upload the new bits.
    const auto size_diff = static_cast<GLintptr>(new_size - old_size);
    glNamedBufferSubData(entry->GetBuffer(), old_size, size_diff, entry->GetHostPtr() + old_size);

    // Update entry's size in the object and in the cache.
    entry->SetSize(new_size);
    Unregister(entry);
    Register(entry);
}

std::shared_ptr<CachedBufferEntry> OGLBufferCache::GetUncachedBuffer(VAddr cpu_addr, u8* host_ptr) {
    if (auto entry = TryGetReservedBuffer(host_ptr); entry) {
        return entry;
    }
    return std::make_shared<CachedBufferEntry>(cpu_addr, host_ptr);
}

std::shared_ptr<CachedBufferEntry> OGLBufferCache::TryGetReservedBuffer(u8* host_ptr) {
    const auto it = buffer_reserve.find(ToCacheAddr(host_ptr));
    if (it == buffer_reserve.end()) {
        return {};
    }
    auto& reserve = it->second;
    auto entry = reserve.back();
    reserve.pop_back();
    return entry;
}

void OGLBufferCache::ReserveBuffer(std::shared_ptr<CachedBufferEntry> entry) {
    buffer_reserve[entry->GetCacheAddr()].push_back(std::move(entry));
}

void OGLBufferCache::AlignBuffer(std::size_t alignment) {
    // Align the offset, not the mapped pointer
    const GLintptr offset_aligned =
        static_cast<GLintptr>(Common::AlignUp(static_cast<std::size_t>(buffer_offset), alignment));
    buffer_ptr += offset_aligned - buffer_offset;
    buffer_offset = offset_aligned;
}

} // namespace OpenGL
