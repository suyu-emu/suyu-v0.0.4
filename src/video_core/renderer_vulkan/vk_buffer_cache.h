// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "video_core/buffer_cache/buffer_cache.h"
#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_stream_buffer.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Vulkan {

class VKDevice;
class VKMemoryManager;
class VKScheduler;

class Buffer final : public VideoCommon::BufferBlock {
public:
    explicit Buffer(const VKDevice& device, VKMemoryManager& memory_manager, VKScheduler& scheduler,
                    VKStagingBufferPool& staging_pool, VAddr cpu_addr_, std::size_t size_);
    ~Buffer();

    void Upload(std::size_t offset, std::size_t data_size, const u8* data);

    void Download(std::size_t offset, std::size_t data_size, u8* data);

    void CopyFrom(const Buffer& src, std::size_t src_offset, std::size_t dst_offset,
                  std::size_t copy_size);

    VkBuffer Handle() const {
        return *buffer.handle;
    }

    u64 Address() const {
        return 0;
    }

private:
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_pool;

    VKBuffer buffer;
};

class VKBufferCache final : public VideoCommon::BufferCache<Buffer, VkBuffer, VKStreamBuffer> {
public:
    explicit VKBufferCache(VideoCore::RasterizerInterface& rasterizer_,
                           Tegra::MemoryManager& gpu_memory_, Core::Memory::Memory& cpu_memory_,
                           const VKDevice& device_, VKMemoryManager& memory_manager_,
                           VKScheduler& scheduler_, VKStagingBufferPool& staging_pool_);
    ~VKBufferCache();

    BufferInfo GetEmptyBuffer(std::size_t size) override;

protected:
    std::shared_ptr<Buffer> CreateBlock(VAddr cpu_addr, std::size_t size) override;

private:
    const VKDevice& device;
    VKMemoryManager& memory_manager;
    VKScheduler& scheduler;
    VKStagingBufferPool& staging_pool;
};

} // namespace Vulkan
