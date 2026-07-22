// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "video_core/renderer_vulkan/present/anti_alias_pass.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;
class StagingBufferPool;

class FXAA final : public AntiAliasPass {
public:
    explicit FXAA(const Device& device, MemoryAllocator& allocator, size_t image_count, VkExtent2D extent);
    ~FXAA() override;

    void Draw(const Device& device, Scheduler& scheduler, size_t image_index, VkImage* inout_image, VkImageView* inout_image_view) override;

private:
    void CreateImages(const Device& device, MemoryAllocator& allocator);
    void CreateRenderPasses(const Device& device);
    void CreateSampler(const Device& device);
    void CreateShaders(const Device& device);
    void CreateDescriptorPool(const Device& device);
    void CreateDescriptorSetLayouts(const Device& device);
    void CreateDescriptorSets(const Device& device);
    void CreatePipelineLayouts(const Device& device);
    void CreatePipelines(const Device& device);
    void UpdateDescriptorSets(const Device& device, VkImageView image_view, size_t image_index);
    void UploadImages(const Device& device, Scheduler& scheduler);

    struct Image {
        vk::DescriptorSets descriptor_sets{};
        vk::Framebuffer framebuffer{};
        vk::Image image{};
        vk::ImageView image_view{};
    };
    std::vector<Image> m_dynamic_images{};
    const VkExtent2D m_extent;
    const u32 m_image_count;
    vk::ShaderModule m_vertex_shader{};
    vk::ShaderModule m_fragment_shader{};
    vk::DescriptorPool m_descriptor_pool{};
    vk::DescriptorSetLayout m_descriptor_set_layout{};
    vk::PipelineLayout m_pipeline_layout{};
    vk::Pipeline m_pipeline{};
    vk::RenderPass m_renderpass{};
    vk::Sampler m_sampler{};
    bool m_images_ready{};
};

} // namespace Vulkan
