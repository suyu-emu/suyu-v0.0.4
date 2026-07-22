// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "common/math_util.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

class Device;
class Scheduler;

class SGSR {
public:
    static constexpr size_t SGSR_STAGE_COUNT = 1;
    explicit SGSR(const Device& device, MemoryAllocator& memory_allocator, size_t image_count, VkExtent2D extent, bool edge_dir);
    VkImageView Draw(const Device& device, Scheduler& scheduler, size_t image_index, VkImage source_image, VkImageView source_image_view, VkExtent2D input_image_extent, const Common::Rectangle<f32>& crop_rect);
private:
    void Initialize(const Device& device);
    void UploadImages(const Device& device, Scheduler& scheduler);
    void UpdateDescriptorSets(const Device& device, VkImageView image_view, size_t image_index);

    MemoryAllocator& m_memory_allocator;
    const size_t m_image_count;
    const VkExtent2D m_extent;

    vk::DescriptorPool m_descriptor_pool;
    vk::DescriptorSetLayout m_descriptor_set_layout;
    vk::PipelineLayout m_pipeline_layout;
    vk::ShaderModule m_vert_shader;
    vk::ShaderModule m_stage_shader;
    vk::Pipeline m_stage_pipeline;
    vk::RenderPass m_renderpass;
    vk::Sampler m_sampler;

    struct Images {
        vk::DescriptorSets descriptor_sets;
        vk::Image image;
        vk::ImageView image_view;
        vk::Framebuffer framebuffer;
    };
    std::vector<Images> m_dynamic_images;
    bool m_images_ready{};
    bool m_edge_dir{};
};

} // namespace Vulkan
