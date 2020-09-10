// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <tuple>

#include "video_core/renderer_vulkan/vk_memory_manager.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/wrapper.h"

namespace Core {
class System;
}

namespace Core::Memory {
class Memory;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace Tegra {
struct FramebufferConfig;
}

namespace VideoCore {
class RasterizerInterface;
}

namespace Vulkan {

struct ScreenInfo;
class RasterizerVulkan;
class VKDevice;
class VKFence;
class VKImage;
class VKScheduler;
class VKSwapchain;

class VKBlitScreen final {
public:
    explicit VKBlitScreen(Core::Memory::Memory& cpu_memory,
                          Core::Frontend::EmuWindow& render_window,
                          VideoCore::RasterizerInterface& rasterizer, const VKDevice& device,
                          VKResourceManager& resource_manager, VKMemoryManager& memory_manager,
                          VKSwapchain& swapchain, VKScheduler& scheduler,
                          const VKScreenInfo& screen_info);
    ~VKBlitScreen();

    void Recreate();

    std::tuple<VKFence&, VkSemaphore> Draw(const Tegra::FramebufferConfig& framebuffer,
                                           bool use_accelerated);

private:
    struct BufferData;

    void CreateStaticResources();
    void CreateShaders();
    void CreateSemaphores();
    void CreateDescriptorPool();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateDescriptorSets();
    void CreatePipelineLayout();
    void CreateGraphicsPipeline();
    void CreateSampler();

    void CreateDynamicResources();
    void CreateFramebuffers();

    void RefreshResources(const Tegra::FramebufferConfig& framebuffer);
    void ReleaseRawImages();
    void CreateStagingBuffer(const Tegra::FramebufferConfig& framebuffer);
    void CreateRawImages(const Tegra::FramebufferConfig& framebuffer);

    void UpdateDescriptorSet(std::size_t image_index, VkImageView image_view) const;
    void SetUniformData(BufferData& data, const Tegra::FramebufferConfig& framebuffer) const;
    void SetVertexData(BufferData& data, const Tegra::FramebufferConfig& framebuffer) const;

    u64 CalculateBufferSize(const Tegra::FramebufferConfig& framebuffer) const;
    u64 GetRawImageOffset(const Tegra::FramebufferConfig& framebuffer,
                          std::size_t image_index) const;

    Core::Memory::Memory& cpu_memory;
    Core::Frontend::EmuWindow& render_window;
    VideoCore::RasterizerInterface& rasterizer;
    const VKDevice& device;
    VKResourceManager& resource_manager;
    VKMemoryManager& memory_manager;
    VKSwapchain& swapchain;
    VKScheduler& scheduler;
    const std::size_t image_count;
    const VKScreenInfo& screen_info;

    vk::ShaderModule vertex_shader;
    vk::ShaderModule fragment_shader;
    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout descriptor_set_layout;
    vk::PipelineLayout pipeline_layout;
    vk::Pipeline pipeline;
    vk::RenderPass renderpass;
    std::vector<vk::Framebuffer> framebuffers;
    vk::DescriptorSets descriptor_sets;
    vk::Sampler sampler;

    vk::Buffer buffer;
    VKMemoryCommit buffer_commit;

    std::vector<std::unique_ptr<VKFenceWatch>> watches;

    std::vector<vk::Semaphore> semaphores;
    std::vector<std::unique_ptr<VKImage>> raw_images;
    std::vector<VKMemoryCommit> raw_buffer_commits;
    u32 raw_width = 0;
    u32 raw_height = 0;
};

} // namespace Vulkan
