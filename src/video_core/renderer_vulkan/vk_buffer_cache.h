// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <unordered_map>
#include <vector>

#include "common/common_types.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"

namespace Core {
class System;
}

namespace Vulkan {

class VKDevice;
class VKMemoryManager;
class VKScheduler;

class CachedBufferBlock final : public VideoCommon::BufferBlock {
public:
    explicit CachedBufferBlock(const VKDevice& device, VKMemoryManager& memory_manager,
                               CacheAddr cache_addr, std::size_t size);
    ~CachedBufferBlock();

    const vk::Buffer* GetHandle() const {
        return &*buffer.handle;
    }

private:
    VKBuffer buffer;
};

using Buffer = std::shared_ptr<CachedBufferBlock>;

class VKBufferCache final : public VideoCommon::BufferCache<Buffer, vk::Buffer, VKStreamBuffer> {
public:
    explicit VKBufferCache(VideoCore::RasterizerInterface& rasterizer, Core::System& system,
                           const VKDevice& device, VKMemoryManager& memory_manager,
                           VKScheduler& scheduler, VKStagingBufferPool& staging_pool);
    ~VKBufferCache();

    const vk::Buffer* GetEmptyBuffer(std::size_t size) override;

protected:
    void WriteBarrier() override {}

    Buffer CreateBlock(CacheAddr cache_addr, std::size_t size) override;

    const vk::Buffer* ToHandle(const Buffer& buffer) override;

    void UploadBlockData(const Buffer& buffer, std::size_t offset, std::size_t size,
                         const u8* data) override;

    void DownloadBlockData(const Buffer& buffer, std::size_t offset, std::size_t size,
                           u8* data) override;

    void CopyBlock(const Buffer& src, const Buffer& dst, std::size_t src_offset,
                   std::size_t dst_offset, std::size_t size) override;

private:
    const VKDevice& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_pool;
};

} // namespace Vulkan
