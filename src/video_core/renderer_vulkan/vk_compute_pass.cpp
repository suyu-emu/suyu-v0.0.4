// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <memory>
#include <optional>
#include <utility>

#include "common/alignment.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/div_ceil.h"
#include "video_core/host_shaders/astc_decoder_comp_spv.h"
#include "video_core/host_shaders/vulkan_quad_indexed_comp_spv.h"
#include "video_core/host_shaders/vulkan_uint8_comp_spv.h"
#include "video_core/renderer_vulkan/vk_compute_pass.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_staging_buffer_pool.h"
#include "video_core/renderer_vulkan/vk_texture_cache.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/texture_cache/accelerated_swizzle.h"
#include "video_core/texture_cache/types.h"
#include "video_core/textures/astc.h"
#include "video_core/textures/decoders.h"
#include "video_core/vulkan_common/vulkan_device.h"
#include "video_core/vulkan_common/vulkan_wrapper.h"

namespace Vulkan {

using Tegra::Texture::SWIZZLE_TABLE;
using Tegra::Texture::ASTC::EncodingsValues;

namespace {

constexpr u32 ASTC_BINDING_SWIZZLE_BUFFER = 0;
constexpr u32 ASTC_BINDING_INPUT_BUFFER = 1;
constexpr u32 ASTC_BINDING_ENC_BUFFER = 2;
constexpr u32 ASTC_BINDING_6_TO_8_BUFFER = 3;
constexpr u32 ASTC_BINDING_7_TO_8_BUFFER = 4;
constexpr u32 ASTC_BINDING_8_TO_8_BUFFER = 5;
constexpr u32 ASTC_BINDING_BYTE_TO_16_BUFFER = 6;
constexpr u32 ASTC_BINDING_OUTPUT_IMAGE = 7;

VkPushConstantRange BuildComputePushConstantRange(std::size_t size) {
    return {
        .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
        .offset = 0,
        .size = static_cast<u32>(size),
    };
}

std::array<VkDescriptorSetLayoutBinding, 2> BuildInputOutputDescriptorSetBindings() {
    return {{
        {
            .binding = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
    }};
}

std::array<VkDescriptorSetLayoutBinding, 8> BuildASTCDescriptorSetBindings() {
    return {{
        {
            .binding = ASTC_BINDING_SWIZZLE_BUFFER, // Swizzle buffer
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_INPUT_BUFFER, // ASTC Img data buffer
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_ENC_BUFFER, // Encodings buffer
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_6_TO_8_BUFFER, // BINDING_6_TO_8_BUFFER
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_7_TO_8_BUFFER, // BINDING_7_TO_8_BUFFER
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_8_TO_8_BUFFER, // BINDING_8_TO_8_BUFFER
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_BYTE_TO_16_BUFFER, // BINDING_BYTE_TO_16_BUFFER
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
        {
            .binding = ASTC_BINDING_OUTPUT_IMAGE, // Output image
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .descriptorCount = 1,
            .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
            .pImmutableSamplers = nullptr,
        },
    }};
}

VkDescriptorUpdateTemplateEntryKHR BuildInputOutputDescriptorUpdateTemplate() {
    return {
        .dstBinding = 0,
        .dstArrayElement = 0,
        .descriptorCount = 2,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .offset = 0,
        .stride = sizeof(DescriptorUpdateEntry),
    };
}

std::array<VkDescriptorUpdateTemplateEntryKHR, 8> BuildASTCPassDescriptorUpdateTemplateEntry() {
    return {{
        {
            .dstBinding = ASTC_BINDING_SWIZZLE_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 0 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_INPUT_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 1 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_ENC_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 2 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_6_TO_8_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 3 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_7_TO_8_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 4 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_8_TO_8_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 5 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_BYTE_TO_16_BUFFER,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            .offset = 6 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
        {
            .dstBinding = ASTC_BINDING_OUTPUT_IMAGE,
            .dstArrayElement = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            .offset = 7 * sizeof(DescriptorUpdateEntry),
            .stride = sizeof(DescriptorUpdateEntry),
        },
    }};
}

struct AstcPushConstants {
    std::array<u32, 2> num_image_blocks;
    std::array<u32, 2> blocks_dims;
    u32 layer;
    VideoCommon::Accelerated::BlockLinearSwizzle2DParams params;
};

struct AstcBufferData {
    decltype(SWIZZLE_TABLE) swizzle_table_buffer = SWIZZLE_TABLE;
    decltype(EncodingsValues) encoding_values = EncodingsValues;
    decltype(REPLICATE_6_BIT_TO_8_TABLE) replicate_6_to_8 = REPLICATE_6_BIT_TO_8_TABLE;
    decltype(REPLICATE_7_BIT_TO_8_TABLE) replicate_7_to_8 = REPLICATE_7_BIT_TO_8_TABLE;
    decltype(REPLICATE_8_BIT_TO_8_TABLE) replicate_8_to_8 = REPLICATE_8_BIT_TO_8_TABLE;
    decltype(REPLICATE_BYTE_TO_16_TABLE) replicate_byte_to_16 = REPLICATE_BYTE_TO_16_TABLE;
} constexpr ASTC_BUFFER_DATA;
} // Anonymous namespace

VKComputePass::VKComputePass(const Device& device, VKDescriptorPool& descriptor_pool,
                             vk::Span<VkDescriptorSetLayoutBinding> bindings,
                             vk::Span<VkDescriptorUpdateTemplateEntryKHR> templates,
                             vk::Span<VkPushConstantRange> push_constants,
                             std::span<const u32> code) {
    descriptor_set_layout = device.GetLogical().CreateDescriptorSetLayout({
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .bindingCount = bindings.size(),
        .pBindings = bindings.data(),
    });
    layout = device.GetLogical().CreatePipelineLayout({
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .setLayoutCount = 1,
        .pSetLayouts = descriptor_set_layout.address(),
        .pushConstantRangeCount = push_constants.size(),
        .pPushConstantRanges = push_constants.data(),
    });
    if (!templates.empty()) {
        descriptor_template = device.GetLogical().CreateDescriptorUpdateTemplateKHR({
            .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR,
            .pNext = nullptr,
            .flags = 0,
            .descriptorUpdateEntryCount = templates.size(),
            .pDescriptorUpdateEntries = templates.data(),
            .templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR,
            .descriptorSetLayout = *descriptor_set_layout,
            .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
            .pipelineLayout = *layout,
            .set = 0,
        });

        descriptor_allocator.emplace(descriptor_pool, *descriptor_set_layout);
    }
    module = device.GetLogical().CreateShaderModule({
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = static_cast<u32>(code.size_bytes()),
        .pCode = code.data(),
    });
    pipeline = device.GetLogical().CreateComputePipeline({
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
                .pNext = nullptr,
                .flags = 0,
                .stage = VK_SHADER_STAGE_COMPUTE_BIT,
                .module = *module,
                .pName = "main",
                .pSpecializationInfo = nullptr,
            },
        .layout = *layout,
        .basePipelineHandle = nullptr,
        .basePipelineIndex = 0,
    });
}

VKComputePass::~VKComputePass() = default;

VkDescriptorSet VKComputePass::CommitDescriptorSet(
    VKUpdateDescriptorQueue& update_descriptor_queue) {
    if (!descriptor_template) {
        return nullptr;
    }
    const VkDescriptorSet set = descriptor_allocator->Commit();
    update_descriptor_queue.Send(*descriptor_template, set);
    return set;
}

Uint8Pass::Uint8Pass(const Device& device, VKScheduler& scheduler_,
                     VKDescriptorPool& descriptor_pool, StagingBufferPool& staging_buffer_pool_,
                     VKUpdateDescriptorQueue& update_descriptor_queue_)
    : VKComputePass(device, descriptor_pool, BuildInputOutputDescriptorSetBindings(),
                    BuildInputOutputDescriptorUpdateTemplate(), {}, VULKAN_UINT8_COMP_SPV),
      scheduler{scheduler_}, staging_buffer_pool{staging_buffer_pool_},
      update_descriptor_queue{update_descriptor_queue_} {}

Uint8Pass::~Uint8Pass() = default;

std::pair<VkBuffer, VkDeviceSize> Uint8Pass::Assemble(u32 num_vertices, VkBuffer src_buffer,
                                                      u32 src_offset) {
    const u32 staging_size = static_cast<u32>(num_vertices * sizeof(u16));
    const auto staging = staging_buffer_pool.Request(staging_size, MemoryUsage::DeviceLocal);

    update_descriptor_queue.Acquire();
    update_descriptor_queue.AddBuffer(src_buffer, src_offset, num_vertices);
    update_descriptor_queue.AddBuffer(staging.buffer, staging.offset, staging_size);
    const VkDescriptorSet set = CommitDescriptorSet(update_descriptor_queue);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([layout = *layout, pipeline = *pipeline, buffer = staging.buffer, set,
                      num_vertices](vk::CommandBuffer cmdbuf) {
        static constexpr u32 DISPATCH_SIZE = 1024;
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        };
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, set, {});
        cmdbuf.Dispatch(Common::DivCeil(num_vertices, DISPATCH_SIZE), 1, 1);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, WRITE_BARRIER);
    });
    return {staging.buffer, staging.offset};
}

QuadIndexedPass::QuadIndexedPass(const Device& device_, VKScheduler& scheduler_,
                                 VKDescriptorPool& descriptor_pool_,
                                 StagingBufferPool& staging_buffer_pool_,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_)
    : VKComputePass(device_, descriptor_pool_, BuildInputOutputDescriptorSetBindings(),
                    BuildInputOutputDescriptorUpdateTemplate(),
                    BuildComputePushConstantRange(sizeof(u32) * 2), VULKAN_QUAD_INDEXED_COMP_SPV),
      scheduler{scheduler_}, staging_buffer_pool{staging_buffer_pool_},
      update_descriptor_queue{update_descriptor_queue_} {}

QuadIndexedPass::~QuadIndexedPass() = default;

std::pair<VkBuffer, VkDeviceSize> QuadIndexedPass::Assemble(
    Tegra::Engines::Maxwell3D::Regs::IndexFormat index_format, u32 num_vertices, u32 base_vertex,
    VkBuffer src_buffer, u32 src_offset) {
    const u32 index_shift = [index_format] {
        switch (index_format) {
        case Tegra::Engines::Maxwell3D::Regs::IndexFormat::UnsignedByte:
            return 0;
        case Tegra::Engines::Maxwell3D::Regs::IndexFormat::UnsignedShort:
            return 1;
        case Tegra::Engines::Maxwell3D::Regs::IndexFormat::UnsignedInt:
            return 2;
        }
        UNREACHABLE();
        return 2;
    }();
    const u32 input_size = num_vertices << index_shift;
    const u32 num_tri_vertices = (num_vertices / 4) * 6;

    const std::size_t staging_size = num_tri_vertices * sizeof(u32);
    const auto staging = staging_buffer_pool.Request(staging_size, MemoryUsage::DeviceLocal);

    update_descriptor_queue.Acquire();
    update_descriptor_queue.AddBuffer(src_buffer, src_offset, input_size);
    update_descriptor_queue.AddBuffer(staging.buffer, staging.offset, staging_size);
    const VkDescriptorSet set = CommitDescriptorSet(update_descriptor_queue);

    scheduler.RequestOutsideRenderPassOperationContext();
    scheduler.Record([layout = *layout, pipeline = *pipeline, buffer = staging.buffer, set,
                      num_tri_vertices, base_vertex, index_shift](vk::CommandBuffer cmdbuf) {
        static constexpr u32 DISPATCH_SIZE = 1024;
        static constexpr VkMemoryBarrier WRITE_BARRIER{
            .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
            .pNext = nullptr,
            .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT,
        };
        const std::array push_constants = {base_vertex, index_shift};
        cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, set, {});
        cmdbuf.PushConstants(layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constants),
                             &push_constants);
        cmdbuf.Dispatch(Common::DivCeil(num_tri_vertices, DISPATCH_SIZE), 1, 1);
        cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                               VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, WRITE_BARRIER);
    });
    return {staging.buffer, staging.offset};
}

using namespace Tegra::Texture::ASTC;
ASTCDecoderPass::ASTCDecoderPass(const Device& device_, VKScheduler& scheduler_,
                                 VKDescriptorPool& descriptor_pool_,
                                 StagingBufferPool& staging_buffer_pool_,
                                 VKUpdateDescriptorQueue& update_descriptor_queue_,
                                 MemoryAllocator& memory_allocator_)
    : VKComputePass(device_, descriptor_pool_, BuildASTCDescriptorSetBindings(),
                    BuildASTCPassDescriptorUpdateTemplateEntry(),
                    BuildComputePushConstantRange(sizeof(AstcPushConstants)),
                    ASTC_DECODER_COMP_SPV),
      device{device_}, scheduler{scheduler_}, staging_buffer_pool{staging_buffer_pool_},
      update_descriptor_queue{update_descriptor_queue_}, memory_allocator{memory_allocator_} {}

ASTCDecoderPass::~ASTCDecoderPass() = default;

void ASTCDecoderPass::MakeDataBuffer() {
    data_buffer = device.GetLogical().CreateBuffer(VkBufferCreateInfo{
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .size = sizeof(ASTC_BUFFER_DATA),
        .usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
        .queueFamilyIndexCount = 0,
        .pQueueFamilyIndices = nullptr,
    });
    data_buffer_commit = memory_allocator.Commit(data_buffer, MemoryUsage::Upload);

    const auto staging_ref =
        staging_buffer_pool.Request(sizeof(ASTC_BUFFER_DATA), MemoryUsage::Upload);
    std::memcpy(staging_ref.mapped_span.data(), &ASTC_BUFFER_DATA, sizeof(ASTC_BUFFER_DATA));
    scheduler.Record([src = staging_ref.buffer, dst = *data_buffer](vk::CommandBuffer cmdbuf) {
        cmdbuf.CopyBuffer(src, dst,
                          VkBufferCopy{
                              .srcOffset = 0,
                              .dstOffset = 0,
                              .size = sizeof(ASTC_BUFFER_DATA),
                          });
        cmdbuf.PipelineBarrier(
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0,
            VkMemoryBarrier{
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
                .pNext = nullptr,
                .srcAccessMask = 0,
                .dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT,
            },
            {}, {});
    });
}

void ASTCDecoderPass::Assemble(Image& image, const StagingBufferRef& map,
                               std::span<const VideoCommon::SwizzleParameters> swizzles) {
    using namespace VideoCommon::Accelerated;
    const VideoCommon::Extent2D tile_size{
        .width = VideoCore::Surface::DefaultBlockWidth(image.info.format),
        .height = VideoCore::Surface::DefaultBlockHeight(image.info.format),
    };
    scheduler.RequestOutsideRenderPassOperationContext();
    if (!data_buffer) {
        MakeDataBuffer();
    }
    const std::array<u32, 2> block_dims{tile_size.width, tile_size.height};
    for (s32 layer = 0; layer < image.info.resources.layers; layer++) {
        for (const VideoCommon::SwizzleParameters& swizzle : swizzles) {
            const size_t input_offset = swizzle.buffer_offset + map.offset;
            const auto num_dispatches_x = Common::DivCeil(swizzle.num_tiles.width, 32U);
            const auto num_dispatches_y = Common::DivCeil(swizzle.num_tiles.height, 32U);
            const std::array num_image_blocks{swizzle.num_tiles.width, swizzle.num_tiles.height};
            const u32 layer_image_size =
                image.guest_size_bytes - static_cast<u32>(swizzle.buffer_offset);

            update_descriptor_queue.Acquire();
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, swizzle_table_buffer),
                                              sizeof(AstcBufferData::swizzle_table_buffer));
            update_descriptor_queue.AddBuffer(map.buffer, input_offset, image.guest_size_bytes);
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, encoding_values),
                                              sizeof(AstcBufferData::encoding_values));
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, replicate_6_to_8),
                                              sizeof(AstcBufferData::replicate_6_to_8));
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, replicate_7_to_8),
                                              sizeof(AstcBufferData::replicate_7_to_8));
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, replicate_8_to_8),
                                              sizeof(AstcBufferData::replicate_8_to_8));
            update_descriptor_queue.AddBuffer(*data_buffer,
                                              offsetof(AstcBufferData, replicate_byte_to_16),
                                              sizeof(AstcBufferData::replicate_byte_to_16));
            update_descriptor_queue.AddImage(image.StorageImageView());

            const VkDescriptorSet set = CommitDescriptorSet(update_descriptor_queue);
            // To unswizzle the ASTC data
            const auto params = MakeBlockLinearSwizzle2DParams(swizzle, image.info);
            scheduler.Record([layout = *layout, pipeline = *pipeline, buffer = map.buffer,
                              num_dispatches_x, num_dispatches_y, layer_image_size,
                              num_image_blocks, block_dims, layer, params, set,
                              image = image.Handle(), input_offset,
                              aspect_mask = image.AspectMask()](vk::CommandBuffer cmdbuf) {
                const AstcPushConstants uniforms{num_image_blocks, block_dims, layer, params};

                cmdbuf.BindPipeline(VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
                cmdbuf.BindDescriptorSets(VK_PIPELINE_BIND_POINT_COMPUTE, layout, 0, set, {});
                cmdbuf.PushConstants(layout, VK_SHADER_STAGE_COMPUTE_BIT, uniforms);
                cmdbuf.Dispatch(num_dispatches_x, num_dispatches_y, 1);

                const VkImageMemoryBarrier image_barrier{
                    .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
                    .pNext = nullptr,
                    .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                    .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
                    .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
                    .newLayout = VK_IMAGE_LAYOUT_GENERAL,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .image = image,
                    .subresourceRange{
                        .aspectMask = aspect_mask,
                        .baseMipLevel = 0,
                        .levelCount = VK_REMAINING_MIP_LEVELS,
                        .baseArrayLayer = 0,
                        .layerCount = VK_REMAINING_ARRAY_LAYERS,
                    },
                };
                cmdbuf.PipelineBarrier(VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                       VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, image_barrier);
            });
        }
    }
}

} // namespace Vulkan
