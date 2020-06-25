// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>

#include "video_core/renderer_vulkan/vk_compute_pipeline.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"
#include "video_core/renderer_vulkan/wrapper.h"

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

VkDescriptorSet VKComputePipeline::CommitDescriptorSet() {
    if (!descriptor_template) {
        return {};
    }
    const auto set = descriptor_allocator.Commit(scheduler.GetFence());
    update_descriptor_queue.Send(*descriptor_template, set);
    return set;
}

vk::DescriptorSetLayout VKComputePipeline::CreateDescriptorSetLayout() const {
    std::vector<VkDescriptorSetLayoutBinding> bindings;
    u32 binding = 0;
    const auto add_bindings = [&](VkDescriptorType descriptor_type, std::size_t num_entries) {
        // TODO(Rodrigo): Maybe make individual bindings here?
        for (u32 bindpoint = 0; bindpoint < static_cast<u32>(num_entries); ++bindpoint) {
            VkDescriptorSetLayoutBinding& entry = bindings.emplace_back();
            entry.binding = binding++;
            entry.descriptorType = descriptor_type;
            entry.descriptorCount = 1;
            entry.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
            entry.pImmutableSamplers = nullptr;
        }
    };
    add_bindings(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, entries.const_buffers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, entries.global_buffers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, entries.uniform_texels.size());
    add_bindings(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, entries.samplers.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, entries.storage_texels.size());
    add_bindings(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, entries.images.size());

    VkDescriptorSetLayoutCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.bindingCount = static_cast<u32>(bindings.size());
    ci.pBindings = bindings.data();
    return device.GetLogical().CreateDescriptorSetLayout(ci);
}

vk::PipelineLayout VKComputePipeline::CreatePipelineLayout() const {
    VkPipelineLayoutCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.setLayoutCount = 1;
    ci.pSetLayouts = descriptor_set_layout.address();
    ci.pushConstantRangeCount = 0;
    ci.pPushConstantRanges = nullptr;
    return device.GetLogical().CreatePipelineLayout(ci);
}

vk::DescriptorUpdateTemplateKHR VKComputePipeline::CreateDescriptorUpdateTemplate() const {
    std::vector<VkDescriptorUpdateTemplateEntryKHR> template_entries;
    u32 binding = 0;
    u32 offset = 0;
    FillDescriptorUpdateTemplateEntries(entries, binding, offset, template_entries);
    if (template_entries.empty()) {
        // If the shader doesn't use descriptor sets, skip template creation.
        return {};
    }

    VkDescriptorUpdateTemplateCreateInfoKHR ci;
    ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_UPDATE_TEMPLATE_CREATE_INFO_KHR;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.descriptorUpdateEntryCount = static_cast<u32>(template_entries.size());
    ci.pDescriptorUpdateEntries = template_entries.data();
    ci.templateType = VK_DESCRIPTOR_UPDATE_TEMPLATE_TYPE_DESCRIPTOR_SET_KHR;
    ci.descriptorSetLayout = *descriptor_set_layout;
    ci.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    ci.pipelineLayout = *layout;
    ci.set = DESCRIPTOR_SET;
    return device.GetLogical().CreateDescriptorUpdateTemplateKHR(ci);
}

vk::ShaderModule VKComputePipeline::CreateShaderModule(const std::vector<u32>& code) const {
    device.SaveShader(code);

    VkShaderModuleCreateInfo ci;
    ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.codeSize = code.size() * sizeof(u32);
    ci.pCode = code.data();
    return device.GetLogical().CreateShaderModule(ci);
}

vk::Pipeline VKComputePipeline::CreatePipeline() const {
    VkComputePipelineCreateInfo ci;
    VkPipelineShaderStageCreateInfo& stage_ci = ci.stage;
    stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage_ci.pNext = nullptr;
    stage_ci.flags = 0;
    stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage_ci.module = *shader_module;
    stage_ci.pName = "main";
    stage_ci.pSpecializationInfo = nullptr;

    VkPipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci;
    subgroup_size_ci.sType =
        VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO_EXT;
    subgroup_size_ci.pNext = nullptr;
    subgroup_size_ci.requiredSubgroupSize = GuestWarpSize;

    if (entries.uses_warps && device.IsGuestWarpSizeSupported(VK_SHADER_STAGE_COMPUTE_BIT)) {
        stage_ci.pNext = &subgroup_size_ci;
    }

    ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    ci.pNext = nullptr;
    ci.flags = 0;
    ci.layout = *layout;
    ci.basePipelineHandle = nullptr;
    ci.basePipelineIndex = 0;
    return device.GetLogical().CreateComputePipeline(ci);
}

} // namespace Vulkan
