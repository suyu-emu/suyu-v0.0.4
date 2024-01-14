// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/math_util.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Layout {
struct FramebufferLayout;
}

namespace Tegra {
struct FramebufferConfig;
}

namespace Vulkan {

class Device;
struct Frame;
class MemoryAllocator;
class Scheduler;

class WindowAdaptPass final {
public:
    explicit WindowAdaptPass(const Device& device, const MemoryAllocator& memory_allocator,
                             size_t num_images, VkFormat frame_format, vk::Sampler&& sampler,
                             vk::ShaderModule&& fragment_shader);
    ~WindowAdaptPass();

    void Draw(Scheduler& scheduler, size_t image_index, VkImageView src_image_view,
              VkExtent2D src_image_extent, const Common::Rectangle<f32>& crop_rect,
              const Layout::FramebufferLayout& layout, Frame* dst);

    VkRenderPass GetRenderPass();

private:
    struct BufferData;

    void SetUniformData(BufferData& data, const Layout::FramebufferLayout& layout) const;
    void SetVertexData(BufferData& data, const Layout::FramebufferLayout& layout,
                       const Common::Rectangle<f32>& crop_rect) const;
    void UpdateDescriptorSet(size_t image_index, VkImageView image_view);
    void ConfigureLayout(size_t image_index, VkImageView image_view,
                         const Layout::FramebufferLayout& layout,
                         const Common::Rectangle<f32>& crop_rect);

    void CreateDescriptorPool(size_t num_images);
    void CreateDescriptorSetLayout();
    void CreateDescriptorSets(size_t num_images);
    void CreatePipelineLayout();
    void CreateVertexShader();
    void CreateRenderPass(VkFormat frame_format);
    void CreatePipeline();
    void CreateBuffer(const MemoryAllocator& memory_allocator);

private:
    const Device& device;
    vk::DescriptorPool descriptor_pool;
    vk::DescriptorSetLayout descriptor_set_layout;
    vk::DescriptorSets descriptor_sets;
    vk::PipelineLayout pipeline_layout;
    vk::Sampler sampler;
    vk::ShaderModule vertex_shader;
    vk::ShaderModule fragment_shader;
    vk::RenderPass render_pass;
    vk::Pipeline pipeline;
    vk::Buffer buffer;
};

} // namespace Vulkan
