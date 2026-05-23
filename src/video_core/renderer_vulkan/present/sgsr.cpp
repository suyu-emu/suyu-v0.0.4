// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/common_types.h"
#include "common/div_ceil.h"
#include "common/settings.h"

//#include "video_core/sgsr.h"
#include "video_core/host_shaders/sgsr1_shader_mobile_frag_spv.h"
#include "video_core/host_shaders/sgsr1_shader_mobile_edge_direction_frag_spv.h"
#include "video_core/host_shaders/sgsr1_shader_vert_spv.h"
#include "video_core/renderer_vulkan/present/sgsr.h"
#include "video_core/renderer_vulkan/present/util.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan {

using PushConstants = std::array<u32, 4 + 2 + 1>;

SGSR::SGSR(const Device& device, MemoryAllocator& memory_allocator, size_t image_count, VkExtent2D extent, bool edge_dir)
    : m_device{device}
    , m_memory_allocator{memory_allocator}
    , m_image_count{image_count}
    , m_extent{extent}
    , m_edge_dir{edge_dir}
{
    // Not finished yet initializing at ctor time?
    m_dynamic_images.resize(m_image_count);
    for (auto& images : m_dynamic_images) {
        images.image = CreateWrappedImage(m_memory_allocator, m_extent, VK_FORMAT_R16G16B16A16_SFLOAT);
        images.image_view = CreateWrappedImageView(m_device, images.image, VK_FORMAT_R16G16B16A16_SFLOAT);
    }

    m_renderpass = CreateWrappedRenderPass(m_device, VK_FORMAT_R16G16B16A16_SFLOAT);
    for (auto& images : m_dynamic_images)
        images.framebuffer = CreateWrappedFramebuffer(m_device, m_renderpass, images.image_view, m_extent);

    m_sampler = CreateBilinearSampler(m_device);
    m_vert_shader = BuildShader(m_device, SGSR1_SHADER_VERT_SPV);
    m_stage_shader = m_edge_dir
        ? BuildShader(m_device, SGSR1_SHADER_MOBILE_EDGE_DIRECTION_FRAG_SPV)
        : BuildShader(m_device, SGSR1_SHADER_MOBILE_FRAG_SPV);
    // 2 descriptors, 2 descriptor sets per invocation
    m_descriptor_pool = CreateWrappedDescriptorPool(m_device,  m_image_count, m_image_count);
    m_descriptor_set_layout = CreateWrappedDescriptorSetLayout(m_device, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER});

    VkDescriptorSetLayout layout = *m_descriptor_set_layout;
    for (auto& images : m_dynamic_images)
        images.descriptor_sets = CreateWrappedDescriptorSets(m_descriptor_pool, layout);

    const VkPushConstantRange range{
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };
    VkPipelineLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = m_descriptor_set_layout.address(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &range,
    };
    m_pipeline_layout = m_device.GetLogical().CreatePipelineLayout(ci);
    m_stage_pipeline = CreateWrappedPipeline(m_device, m_renderpass, m_pipeline_layout, std::tie(m_vert_shader, m_stage_shader));
}

void SGSR::UpdateDescriptorSets(VkImageView image_view, size_t image_index) {
    Images& images = m_dynamic_images[image_index];
    std::vector<VkDescriptorImageInfo> image_infos;
    std::vector<VkWriteDescriptorSet> updates;
    image_infos.reserve(1);
    updates.push_back(CreateWriteDescriptorSet(image_infos, *m_sampler, image_view, images.descriptor_sets[0], 0));
    m_device.GetLogical().UpdateDescriptorSets(updates, {});
}

void SGSR::UploadImages(Scheduler& scheduler) {
    if (!m_images_ready) {
        scheduler.Record([&](vk::CommandBuffer cmdbuf) {
            for (auto& image : m_dynamic_images)
                ClearColorImage(cmdbuf, *image.image);
        });
        scheduler.Finish();
        m_images_ready = true;
    }
}

VkImageView SGSR::Draw(Scheduler& scheduler, size_t image_index, VkImage source_image, VkImageView source_image_view, VkExtent2D input_image_extent, const Common::Rectangle<f32>& crop_rect) {
    Images& images = m_dynamic_images[image_index];
    auto const output_image = *images.image;
    auto const descriptor_set = images.descriptor_sets[0];
    auto const framebuffer = *images.framebuffer;
    auto const pipeline = *m_stage_pipeline;

    VkPipelineLayout layout = *m_pipeline_layout;
    VkRenderPass renderpass = *m_renderpass;
    VkExtent2D extent = m_extent;

    const f32 input_image_width = f32(input_image_extent.width);
    const f32 input_image_height = f32(input_image_extent.height);
    const f32 viewport_width = (crop_rect.right - crop_rect.left) * input_image_width;
    const f32 viewport_height = (crop_rect.bottom - crop_rect.top) * input_image_height;
    // expected [0, 2]
    const f32 sharpening = f32(Settings::values.fsr_sharpening_slider.GetValue()) / 100.0f;

    // p = (tex * viewport) / input = [0,n] (normalized texcoords)
    // p * input = [0,1024], [0,768]
    // layout( push_constant ) uniform constants {
    //     highp vec4 ViewportInfo[1];
    //     highp vec2 ResizeFactor;
    //     highp float EdgeSharpness;
    // };
    PushConstants viewport_con{};
    viewport_con[0] = std::bit_cast<u32>(std::abs(1.f / viewport_width));
    viewport_con[1] = std::bit_cast<u32>(std::abs(1.f / viewport_height));
    viewport_con[2] = std::bit_cast<u32>(std::abs(viewport_width));
    viewport_con[3] = std::bit_cast<u32>(std::abs(viewport_height));
    viewport_con[4] = std::bit_cast<u32>(viewport_width / input_image_width);
    viewport_con[5] = std::bit_cast<u32>(viewport_height / input_image_height);
    viewport_con[6] = std::bit_cast<u32>(sharpening);

    UploadImages(scheduler);
    UpdateDescriptorSets(source_image_view, image_index);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([=](vk::CommandBuffer cmdbuf) {
        TransitionImageLayout(cmdbuf, source_image, VK_IMAGE_LAYOUT_GENERAL);
        TransitionImageLayout(cmdbuf, output_image, VK_IMAGE_LAYOUT_GENERAL);
        BeginRenderPass(cmdbuf, renderpass, framebuffer, extent);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, layout, 0, descriptor_set, {});
        cmdbuf.PushConstants(layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, viewport_con);
        cmdbuf.Draw(3, 1, 0, 0);
        cmdbuf.EndRenderPass();
        TransitionImageLayout(cmdbuf, output_image, VK_IMAGE_LAYOUT_GENERAL);
    });
    return *images.image_view;
}

} // namespace Vulkan
