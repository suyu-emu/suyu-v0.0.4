// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>

#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"

namespace Vulkan {

VKComputePipeline::VKComputePipeline(const VKDevice& device, VKScheduler& scheduler,
                                     VKDescriptorPool& descriptor_pool,
                                     VKUpdateDescriptorQueue& update_descriptor_queue,
                                     const SPIRVShader& shader)
    : device{device}, scheduler{scheduler}, entries{shader.entries},
      descriptor_set_layout{CreateDescriptorSetLayout()},
      descriptor_allocator{descriptor_pool, *descriptor_set_layout},
      update_descriptor_queue{update_descriptor_queue}, layout{CreatePipelineLayout()},
      descriptor_template{CreateDescriptorUpdateTemplate()},
      shader_module{CreateShaderModule(shader.code)}, pipeline{CreatePipeline()} {}

VKComputePipeline::~VKComputePipeline() = default;

vk::DescriptorSet VKComputePipeline::CommitDescriptorSet() {
    if (!descriptor_template) {
        return {};
    }
    const auto set = descriptor_allocator.Commit(scheduler.GetFence());
    update_descriptor_queue.Send(*descriptor_template, set);
    return set;
}

UniqueDescriptorSetLayout VKComputePipeline::CreateDescriptorSetLayout() const {
    std::vector<vk::DescriptorSetLayoutBinding> bindings;
    u32 binding = 0;
    const auto AddBindings = [&](vk::DescriptorType descriptor_type, std::size_t num_entries) {
        // TODO(Rodrigo): Maybe make individual bindings here?
        for (u32 bindpoint = 0; bindpoint < static_cast<u32>(num_entries); ++bindpoint) {
            bindings.emplace_back(binding++, descriptor_type, 1, vk::ShaderStageFlagBits::eCompute,
                                  nullptr);
        }
    };
    AddBindings(vk::DescriptorType::eUniformBuffer, entries.const_buffers.size());
    AddBindings(vk::DescriptorType::eStorageBuffer, entries.global_buffers.size());
    AddBindings(vk::DescriptorType::eUniformTexelBuffer, entries.texel_buffers.size());
    AddBindings(vk::DescriptorType::eCombinedImageSampler, entries.samplers.size());
    AddBindings(vk::DescriptorType::eStorageImage, entries.images.size());

    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci(
        {}, static_cast<u32>(bindings.size()), bindings.data());

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createDescriptorSetLayoutUnique(descriptor_set_layout_ci, nullptr, dld);
}

UniquePipelineLayout VKComputePipeline::CreatePipelineLayout() const {
    const vk::PipelineLayoutCreateInfo layout_ci({}, 1, &*descriptor_set_layout, 0, nullptr);
    const auto dev = device.GetLogical();
    return dev.createPipelineLayoutUnique(layout_ci, nullptr, device.GetDispatchLoader());
}

UniqueDescriptorUpdateTemplate VKComputePipeline::CreateDescriptorUpdateTemplate() const {
    std::vector<vk::DescriptorUpdateTemplateEntry> template_entries;
    u32 binding = 0;
    u32 offset = 0;
    FillDescriptorUpdateTemplateEntries(entries, binding, offset, template_entries);
    if (template_entries.empty()) {
        // If the shader doesn't use descriptor sets, skip template creation.
        return UniqueDescriptorUpdateTemplate{};
    }

    const vk::DescriptorUpdateTemplateCreateInfo template_ci(
        {}, static_cast<u32>(template_entries.size()), template_entries.data(),
        vk::DescriptorUpdateTemplateType::eDescriptorSet, *descriptor_set_layout,
        vk::PipelineBindPoint::eGraphics, *layout, DESCRIPTOR_SET);

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createDescriptorUpdateTemplateUnique(template_ci, nullptr, dld);
}

UniqueShaderModule VKComputePipeline::CreateShaderModule(const std::vector<u32>& code) const {
    const vk::ShaderModuleCreateInfo module_ci({}, code.size() * sizeof(u32), code.data());
    const auto dev = device.GetLogical();
    return dev.createShaderModuleUnique(module_ci, nullptr, device.GetDispatchLoader());
}

UniquePipeline VKComputePipeline::CreatePipeline() const {
    vk::PipelineShaderStageCreateInfo shader_stage_ci({}, vk::ShaderStageFlagBits::eCompute,
                                                      *shader_module, "main", nullptr);
    vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci;
    subgroup_size_ci.requiredSubgroupSize = GuestWarpSize;
    if (entries.uses_warps && device.IsGuestWarpSizeSupported(vk::ShaderStageFlagBits::eCompute)) {
        shader_stage_ci.pNext = &subgroup_size_ci;
    }

    const vk::ComputePipelineCreateInfo create_info({}, shader_stage_ci, *layout, {}, 0);
    const auto dev = device.GetLogical();
    return dev.createComputePipelineUnique({}, create_info, nullptr, device.GetDispatchLoader());
}

} // namespace Vulkan
