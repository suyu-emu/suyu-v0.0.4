// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include "video_core/renderer_vulkan/present/anti_alias_pass.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;
class StagingBufferPool;

class SMAA final : public AntiAliasPass {
public:
    explicit SMAA(const Device& device, MemoryAllocator& allocator, size_t image_count, VkExtent2D extent);
    ~SMAA() override;

    void Draw(const Device& device, Scheduler& scheduler, size_t image_index, VkImage* inout_image,
              VkImageView* inout_image_view) override;

private:
    enum SMAAStage {
        EdgeDetection = 0,
        BlendingWeightCalculation = 1,
        NeighborhoodBlending = 2,
        MaxSMAAStage = 3,
    };

    enum StaticImageType {
        Area = 0,
        Search = 1,
        MaxStaticImage = 2,
    };

    enum DynamicImageType {
        Blend = 0,
        Edges = 1,
        Output = 2,
        MaxDynamicImage = 3,
    };

    void CreateImages(const Device& device);
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

    MemoryAllocator& m_allocator;
    const VkExtent2D m_extent;
    const u32 m_image_count;

    vk::DescriptorPool m_descriptor_pool{};
    std::array<vk::DescriptorSetLayout, MaxSMAAStage> m_descriptor_set_layouts{};
    std::array<vk::PipelineLayout, MaxSMAAStage> m_pipeline_layouts{};
    std::array<vk::ShaderModule, MaxSMAAStage> m_vertex_shaders{};
    std::array<vk::ShaderModule, MaxSMAAStage> m_fragment_shaders{};
    std::array<vk::Pipeline, MaxSMAAStage> m_pipelines{};
    std::array<vk::RenderPass, MaxSMAAStage> m_renderpasses{};

    std::array<vk::Image, MaxStaticImage> m_static_images{};
    std::array<vk::ImageView, MaxStaticImage> m_static_image_views{};

    struct Images {
        vk::DescriptorSets descriptor_sets{};
        std::array<vk::Image, MaxDynamicImage> images{};
        std::array<vk::ImageView, MaxDynamicImage> image_views{};
        std::array<vk::Framebuffer, MaxSMAAStage> framebuffers{};
    };
    std::vector<Images> m_dynamic_images{};
    bool m_images_ready{};

    vk::Sampler m_sampler{};
};

} // namespace Vulkan
