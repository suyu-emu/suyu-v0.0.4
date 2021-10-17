// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/div_ceil.h"
#include "video_core/host_shaders/vulkan_fidelityfx_fsr_easu_comp_spv.h"
#include "video_core/host_shaders/vulkan_fidelityfx_fsr_rcas_comp_spv.h"
#include "video_core/renderer_vulkan/vk_fsr.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_util.h"
#include "video_core/vulkan_common/vulkan_device.h"

namespace Vulkan {

FSR::FSR(const Device& device, MemoryAllocator& memory_allocator, size_t image_count,
         VkExtent2D output_size)
    : device{device}, memory_allocator{memory_allocator}, image_count{image_count},
      output_size{output_size} {

    CreateImages();
    CreateSampler();
    CreateShaders();
    CreateDescriptorPool();
    CreateDescriptorSetLayout();
    CreateDescriptorSets();
    CreatePipelineLayout();
    CreatePipeline();
}

VkImageView FSR::Draw(VKScheduler& scheduler, size_t image_index, VkImageView image_view,
                      const Common::Rectangle<int>& crop_rect) {

    UpdateDescriptorSet(image_index, image_view);

    scheduler.Record([this, image_index, crop_rect](vk::CommandBuffer cmdbuf) {
        const VkImageMemoryBarrier base_barrier{
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = 0,
            .dstAccessMask = 0,
            .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
            .newLayout = VK_IMAGE_LAYOUT_GENERAL,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .image = {},
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        };

        // TODO: Support clear color
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, *easu_pipeline);
        cmdbuf.PushConstants(*pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT,
                             VkExtent2D{
                                 .width = static_cast<u32>(crop_rect.GetWidth()),
                                 .height = static_cast<u32>(crop_rect.GetHeight()),
                             });

        {
            VkImageMemoryBarrier fsr_write_barrier = base_barrier;
            fsr_write_barrier.image = *images[image_index],
            fsr_write_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;

            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, fsr_write_barrier);
        }

        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline_layout, 0,
                                  descriptor_sets[image_index * 2], {});
        cmdbuf.Dispatch(Common::DivCeil(output_size.width, 16u),
                        Common::DivCeil(output_size.height, 16u), 1);

        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, *rcas_pipeline);
        cmdbuf.PushConstants(*pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, output_size);

        {
            std::array<VkImageMemoryBarrier, 2> barriers;
            auto& fsr_read_barrier = barriers[0];
            auto& blit_write_barrier = barriers[1];

            fsr_read_barrier = base_barrier;
            fsr_read_barrier.image = *images[image_index];
            fsr_read_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            fsr_read_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            blit_write_barrier = base_barrier;
            blit_write_barrier.image = *images[image_count + image_index];
            blit_write_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            blit_write_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;

            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, {}, {}, barriers);
        }

        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, *pipeline_layout, 0,
                                  descriptor_sets[image_index * 2 + 1], {});
        cmdbuf.Dispatch(Common::DivCeil(output_size.width, 16u),
                        Common::DivCeil(output_size.height, 16u), 1);

        {
            std::array<VkImageMemoryBarrier, 1> barriers;
            auto& blit_read_barrier = barriers[0];

            blit_read_barrier = base_barrier;
            blit_read_barrier.image = *images[image_count + image_index];
            blit_read_barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            blit_read_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                   VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, {}, {}, barriers);
        }
    });

    return *image_views[image_count + image_index];
}

void FSR::CreateDescriptorPool() {
    const std::array<VkDescriptorPoolSize, 2> pool_sizes{{
        {
            .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = static_cast<u32>(image_count * 2),
        },
        {
            .type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = static_cast<u32>(image_count * 2),
        },
    }};

    const VkDescriptorPoolCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = static_cast<u32>(image_count * 2),
        .poolSizeCount = static_cast<u32>(pool_sizes.size()),
        .pPoolSizes = pool_sizes.data(),
    };
    descriptor_pool = device.GetLogical().CreateDescriptorPool(ci);
}

void FSR::CreateDescriptorSetLayout() {
    const std::array<VkDescriptorSetLayoutBinding, 2> layout_bindings{{
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = sampler.address(),
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = sampler.address(),
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

void FSR::CreateDescriptorSets() {
    const u32 sets = static_cast<u32>(image_count * 2);
    const std::vector layouts(sets, *descriptor_set_layout);

    const VkDescriptorSetAllocateInfo ai{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = nullptr,
        .descriptorPool = *descriptor_pool,
        .descriptorSetCount = sets,
        .pSetLayouts = layouts.data(),
    };

    descriptor_sets = descriptor_pool.Allocate(ai);
}

void FSR::CreateImages() {
    images.resize(image_count * 2);
    image_views.resize(image_count * 2);
    buffer_commits.resize(image_count * 2);

    for (size_t i = 0; i < image_count * 2; ++i) {
        images[i] = device.GetLogical().CreateImage(VkImageCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .extent =
                {
                    .width = output_size.width,
                    .height = output_size.height,
                    .depth = 1,
                },
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT |
                     VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        });
        buffer_commits[i] = memory_allocator.Commit(images[i], MemoryUsage::DeviceLocal);
        image_views[i] = device.GetLogical().CreateImageView(VkImageViewCreateInfo{
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .image = *images[i],
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = VK_FORMAT_R16G16B16A16_SFLOAT,
            .components =
                {
                    .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                    .a = VK_COMPONENT_SWIZZLE_IDENTITY,
                },
            .subresourceRange =
                {
                    .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                    .baseMipLevel = 0,
                    .levelCount = 1,
                    .baseArrayLayer = 0,
                    .layerCount = 1,
                },
        });
    }
}

void FSR::CreatePipelineLayout() {
    VkPushConstantRange push_const{
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = sizeof(std::array<u32, 2>),
    };
    VkPipelineLayoutCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptor_set_layout.address(),
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_const,
    };

    pipeline_layout = device.GetLogical().CreatePipelineLayout(ci);
}

void FSR::UpdateDescriptorSet(std::size_t image_index, VkImageView image_view) const {
    const auto fsr_image_view = *image_views[image_index];
    const auto blit_image_view = *image_views[image_count + image_index];

    const VkDescriptorImageInfo image_info{
        .imageView = image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkDescriptorImageInfo fsr_image_info{
        .imageView = fsr_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };
    const VkDescriptorImageInfo blit_image_info{
        .imageView = blit_image_view,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    };

    VkWriteDescriptorSet sampler_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index * 2],
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    VkWriteDescriptorSet output_write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[image_index * 2],
        .dstBinding = 1,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
        .pImageInfo = &fsr_image_info,
        .pBufferInfo = nullptr,
        .pTexelBufferView = nullptr,
    };

    device.GetLogical().UpdateDescriptorSets(std::array{sampler_write, output_write}, {});

    sampler_write.dstSet = descriptor_sets[image_index * 2 + 1];
    sampler_write.pImageInfo = &fsr_image_info;
    output_write.dstSet = descriptor_sets[image_index * 2 + 1];
    output_write.pImageInfo = &blit_image_info;

    device.GetLogical().UpdateDescriptorSets(std::array{sampler_write, output_write}, {});
}

void FSR::CreateSampler() {
    const VkSamplerCreateInfo ci{
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .magFilter = VK_FILTER_LINEAR,
        .minFilter = VK_FILTER_LINEAR,
        .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
        .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
        .mipLodBias = 0.0f,
        .anisotropyEnable = VK_FALSE,
        .maxAnisotropy = 0.0f,
        .compareEnable = VK_FALSE,
        .compareOp = VK_COMPARE_OP_NEVER,
        .minLod = 0.0f,
        .maxLod = 0.0f,
        .borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK,
        .unnormalizedCoordinates = VK_FALSE,
    };

    sampler = device.GetLogical().CreateSampler(ci);
}

void FSR::CreateShaders() {
    easu_shader = BuildShader(device, VULKAN_FIDELITYFX_FSR_EASU_COMP_SPV);
    rcas_shader = BuildShader(device, VULKAN_FIDELITYFX_FSR_EASU_COMP_SPV);
}

void FSR::CreatePipeline() {
    VkPipelineShaderStageCreateInfo shader_stage{

        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = VK_SHADER_STAGE_COMPUTE_BIT,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    VkComputePipelineCreateInfo pipeline_ci{
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .layout = *pipeline_layout,
        .basePipelineIndex = 0,
    };

    shader_stage.module = *easu_shader;
    pipeline_ci.stage = shader_stage;
    easu_pipeline = device.GetLogical().CreateComputePipeline(pipeline_ci);

    shader_stage.module = *rcas_shader;
    pipeline_ci.stage = shader_stage;
    rcas_pipeline = device.GetLogical().CreateComputePipeline(pipeline_ci);
}

} // namespace Vulkan
