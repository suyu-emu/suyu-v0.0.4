// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <cstring>

#include "core/frontend/framebuffer_layout.h"
#include "video_core/host_shaders/vulkan_present_vert_spv.h"
#include "video_core/renderer_vulkan/present/util.h"
#include "video_core/renderer_vulkan/present/window_adapt_pass.h"
#include "video_core/renderer_vulkan/vk_present_manager.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_memory_allocator.h"

namespace Vulkan {

namespace {

struct ScreenRectVertex {
    ScreenRectVertex() = default;
    explicit ScreenRectVertex(f32 x, f32 y, f32 u, f32 v) : position{{x, y}}, tex_coord{{u, v}} {}

    std::array<f32, 2> position;
    std::array<f32, 2> tex_coord;

    static VkVertexInputBindingDescription GetDescription() {
        return {
            .binding = 0,
            .stride = sizeof(ScreenRectVertex),
            .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
        };
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributes() {
        return {{
            {
                .location = 0,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ScreenRectVertex, position),
            },
            {
                .location = 1,
                .binding = 0,
                .format = VK_FORMAT_R32G32_SFLOAT,
                .offset = offsetof(ScreenRectVertex, tex_coord),
            },
        }};
    }
};

std::array<f32, 4 * 4> MakeOrthographicMatrix(f32 width, f32 height) {
    // clang-format off
    return { 2.f / width, 0.f,          0.f, 0.f,
             0.f,         2.f / height, 0.f, 0.f,
             0.f,         0.f,          1.f, 0.f,
            -1.f,        -1.f,          0.f, 1.f};
    // clang-format on
}

} // Anonymous namespace

struct WindowAdaptPass::BufferData {
    struct {
        std::array<f32, 4 * 4> modelview_matrix;
    } uniform;

    std::array<ScreenRectVertex, 4> vertices;
};

WindowAdaptPass::WindowAdaptPass(const Device& device_, const MemoryAllocator& memory_allocator,
                                 size_t num_images, VkFormat frame_format, vk::Sampler&& sampler_,
                                 vk::ShaderModule&& fragment_shader_)
    : device(device_), sampler(std::move(sampler_)), fragment_shader(std::move(fragment_shader_)) {
    CreateDescriptorPool(num_images);
    CreateDescriptorSetLayout();
    CreateDescriptorSets(num_images);
    CreatePipelineLayout();
    CreateVertexShader();
    CreateRenderPass(frame_format);
    CreatePipeline();
    CreateBuffer(memory_allocator);
}

WindowAdaptPass::~WindowAdaptPass() = default;

void WindowAdaptPass::Draw(Scheduler& scheduler, size_t image_index, VkImageView src_image_view,
                           VkExtent2D src_image_extent, const Common::Rectangle<f32>& crop_rect,
                           const Layout::FramebufferLayout& layout, Frame* dst) {
    ConfigureLayout(image_index, src_image_view, layout, crop_rect);

    const VkFramebuffer host_framebuffer{*dst->framebuffer};
    const VkRenderPass renderpass{*render_pass};
    const VkPipeline graphics_pipeline{*pipeline};
    const VkDescriptorSet descriptor_set{descriptor_sets[image_index]};
    const VkExtent2D render_area{
        .width = dst->width,
        .height = dst->height,
    };

    scheduler.Record([=](vk::CommandBuffer cmdbuf) {
        const f32 bg_red = Settings::values.bg_red.GetValue() / 255.0f;
        const f32 bg_green = Settings::values.bg_green.GetValue() / 255.0f;
        const f32 bg_blue = Settings::values.bg_blue.GetValue() / 255.0f;
        const VkClearValue clear_color{
            .color = {.float32 = {bg_red, bg_green, bg_blue, 1.0f}},
        };
        const VkRenderPassBeginInfo renderpass_bi{
            .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
            .pNext = nullptr,
            .renderPass = renderpass,
            .framebuffer = host_framebuffer,
            .renderArea =
                {
                    .offset = {0, 0},
                    .extent = render_area,
                },
            .clearValueCount = 1,
            .pClearValues = &clear_color,
        };
        const VkViewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>(render_area.width),
            .height = static_cast<float>(render_area.height),
            .minDepth = 0.0f,
            .maxDepth = 1.0f,
        };
        const VkRect2D scissor{
            .offset = {0, 0},
            .extent = render_area,
        };
        cmdbuf.BeginRenderPass(renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);
        cmdbuf.SetViewport(0, viewport);
        cmdbuf.SetScissor(0, scissor);
        cmdbuf.BindVertexBuffer(0, *buffer, offsetof(BufferData, vertices));
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_GRAPHICS, *pipeline_layout, 0,
                                  descriptor_set, {});
        cmdbuf.Draw(4, 1, 0, 0);
        cmdbuf.EndRenderPass();
    });
}

VkRenderPass WindowAdaptPass::GetRenderPass() {
    return *render_pass;
}

void WindowAdaptPass::CreateDescriptorPool(size_t num_images) {
    const std::array<VkDescriptorPoolSize, 2> pool_sizes{{
        {
            .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = static_cast<u32>(num_images),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<u32>(num_images),
        },
    }};

    const VkDescriptorPoolCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .maxSets = static_cast<u32>(num_images),
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    descriptor_pool = device.GetLogical().CreateDescriptorPool(ci);
}

void WindowAdaptPass::CreateDescriptorSetLayout() {
    const std::array<VkDescriptorSetLayoutBinding, 2> layout_bindings{{
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
            .pImmutableSamplers = nullptr,
        },
    }};

    const VkDescriptorSetLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = static_cast<u32>(layout_bindings.size()),
        .pBindings = layout_bindings.data(),
    };

    descriptor_set_layout = device.GetLogical().CreateDescriptorSetLayout(ci);
}

void WindowAdaptPass::CreateDescriptorSets(size_t num_images) {
    const std::vector layouts(num_images, *descriptor_set_layout);
    descriptor_sets = CreateWrappedDescriptorSets(descriptor_pool, layouts);
}

void WindowAdaptPass::CreateBuffer(const MemoryAllocator& memory_allocator) {
    const VkBufferCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sizeof(BufferData),
        .usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                 VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    };

    buffer = memory_allocator.CreateBuffer(ci, MemoryUsage::Upload);
}

void WindowAdaptPass::CreateRenderPass(VkFormat frame_format) {
    const VkAttachmentDescription color_attachment{
        .flags = 0,
        .format = frame_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkAttachmentReference color_attachment_ref{
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkSubpassDescription subpass_description{
        .flags = 0,
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .inputAttachmentCount = 0,
        .pInputAttachments = nullptr,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
        .pResolveAttachments = nullptr,
        .pDepthStencilAttachment = nullptr,
        .preserveAttachmentCount = 0,
        .pPreserveAttachments = nullptr,
    };

    const VkSubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dependencyFlags = 0,
    };

    const VkRenderPassCreateInfo renderpass_ci{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass_description,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    render_pass = device.GetLogical().CreateRenderPass(renderpass_ci);
}

void WindowAdaptPass::CreateVertexShader() {
    vertex_shader = BuildShader(device, VULKAN_PRESENT_VERT_SPV);
}

void WindowAdaptPass::CreatePipelineLayout() {
    const VkPipelineLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptor_set_layout.address(),
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = nullptr,
    };
    pipeline_layout = device.GetLogical().CreatePipelineLayout(ci);
}

void WindowAdaptPass::SetUniformData(BufferData& data,
                                     const Layout::FramebufferLayout& layout) const {
    data.uniform.modelview_matrix =
        MakeOrthographicMatrix(static_cast<f32>(layout.width), static_cast<f32>(layout.height));
}

void WindowAdaptPass::SetVertexData(BufferData& data, const Layout::FramebufferLayout& layout,
                                    const Common::Rectangle<f32>& crop) const {
    // Map the coordinates to the screen.
    const auto& screen = layout.screen;
    const auto x = static_cast<f32>(screen.left);
    const auto y = static_cast<f32>(screen.top);
    const auto w = static_cast<f32>(screen.GetWidth());
    const auto h = static_cast<f32>(screen.GetHeight());

    data.vertices[0] = ScreenRectVertex(x, y, crop.left, crop.top);
    data.vertices[1] = ScreenRectVertex(x + w, y, crop.right, crop.top);
    data.vertices[2] = ScreenRectVertex(x, y + h, crop.left, crop.bottom);
    data.vertices[3] = ScreenRectVertex(x + w, y + h, crop.right, crop.bottom);
}

void WindowAdaptPass::UpdateDescriptorSet(size_t image_index, VkImageView image_view) {
    const VkDescriptorBufferInfo buffer_info{
        .buffer = *buffer,
        .offset = offsetof(BufferData, uniform),
        .range = sizeof(BufferData::uniform),
    };

    const VkWriteDescriptorSet ubo_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_info,
        .pTexelBufferView = nullptr,
    };

    const VkDescriptorImageInfo image_info{
        .sampler = *sampler,
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    const VkWriteDescriptorSet sampler_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    device.GetLogical().UpdateDescriptorSets(std::array{ubo_write, sampler_write}, {});
}

void WindowAdaptPass::ConfigureLayout(size_t image_index, VkImageView image_view,
                                      const Layout::FramebufferLayout& layout,
                                      const Common::Rectangle<f32>& crop_rect) {
    BufferData data;
    SetUniformData(data, layout);
    SetVertexData(data, layout, crop_rect);

    const std::span<u8> mapped_span = buffer.Mapped();
    std::memcpy(mapped_span.data(), &data, sizeof(data));

    UpdateDescriptorSet(image_index, image_view);
}

void WindowAdaptPass::CreatePipeline() {
    const std::array<VkPipelineShaderStageCreateInfo, 2> shader_stages{{
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_VERTEX_BIT,
            .module = *vertex_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
        {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
            .module = *fragment_shader,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        },
    }};

    const auto vertex_binding_description = ScreenRectVertex::GetDescription();
    const auto vertex_attrs_description = ScreenRectVertex::GetAttributes();

    const VkPipelineVertexInputStateCreateInfo vertex_input_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .vertexBindingDescriptionCount = 1,
        .pVertexBindingDescriptions = &vertex_binding_description,
        .vertexAttributeDescriptionCount = u32{vertex_attrs_description.size()},
        .pVertexAttributeDescriptions = vertex_attrs_description.data(),
    };

    const VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .primitiveRestartEnable = VK_FALSE,
    };

    const VkPipelineViewportStateCreateInfo viewport_state_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = nullptr,
        .scissorCount = 1,
        .pScissors = nullptr,
    };

    const VkPipelineRasterizationStateCreateInfo rasterization_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
        .lineWidth = 1.0f,
    };

    const VkPipelineMultisampleStateCreateInfo multisampling_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 0.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    const VkPipelineColorBlendAttachmentState color_blend_attachment{
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                          VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
    };

    const VkPipelineColorBlendStateCreateInfo color_blend_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
    };

    static constexpr std::array dynamic_states{
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    const VkPipelineDynamicStateCreateInfo dynamic_state_ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = static_cast<u32>(dynamic_states.size()),
        .pDynamicStates = dynamic_states.data(),
    };

    const VkGraphicsPipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stageCount = static_cast<u32>(shader_stages.size()),
        .pStages = shader_stages.data(),
        .pVertexInputState = &vertex_input_ci,
        .pInputAssemblyState = &input_assembly_ci,
        .pTessellationState = nullptr,
        .pViewportState = &viewport_state_ci,
        .pRasterizationState = &rasterization_ci,
        .pMultisampleState = &multisampling_ci,
        .pDepthStencilState = nullptr,
        .pColorBlendState = &color_blend_ci,
        .pDynamicState = &dynamic_state_ci,
        .layout = *pipeline_layout,
        .renderPass = *render_pass,
        .subpass = 0,
        .basePipelineHandle = 0,
        .basePipelineIndex = 0,
    };

    pipeline = device.GetLogical().CreateGraphicsPipeline(pipeline_ci);
}

} // namespace Vulkan
