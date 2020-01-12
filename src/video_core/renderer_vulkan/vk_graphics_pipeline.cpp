// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <vector>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/microprofile.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/maxwell_to_vk.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_device.h"
#include "video_core/renderer_vulkan/vk_graphics_pipeline.h"
#include "video_core/renderer_vulkan/vk_pipeline_cache.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/vk_scheduler.h"
#include "video_core/renderer_vulkan/vk_update_descriptor.h"

namespace Vulkan {

MICROPROFILE_DECLARE(Vulkan_PipelineCache);

namespace {

vk::StencilOpState GetStencilFaceState(const FixedPipelineState::StencilFace& face) {
    return vk::StencilOpState(MaxwellToVK::StencilOp(face.action_stencil_fail),
                              MaxwellToVK::StencilOp(face.action_depth_pass),
                              MaxwellToVK::StencilOp(face.action_depth_fail),
                              MaxwellToVK::ComparisonOp(face.test_func), 0, 0, 0);
}

bool SupportsPrimitiveRestart(vk::PrimitiveTopology topology) {
    static constexpr std::array unsupported_topologies = {
        vk::PrimitiveTopology::ePointList,
        vk::PrimitiveTopology::eLineList,
        vk::PrimitiveTopology::eTriangleList,
        vk::PrimitiveTopology::eLineListWithAdjacency,
        vk::PrimitiveTopology::eTriangleListWithAdjacency,
        vk::PrimitiveTopology::ePatchList};
    return std::find(std::begin(unsupported_topologies), std::end(unsupported_topologies),
                     topology) == std::end(unsupported_topologies);
}

} // Anonymous namespace

VKGraphicsPipeline::VKGraphicsPipeline(const VKDevice& device, VKScheduler& scheduler,
                                       VKDescriptorPool& descriptor_pool,
                                       VKUpdateDescriptorQueue& update_descriptor_queue,
                                       VKRenderPassCache& renderpass_cache,
                                       const GraphicsPipelineCacheKey& key,
                                       const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                                       const SPIRVProgram& program)
    : device{device}, scheduler{scheduler}, fixed_state{key.fixed_state}, hash{key.Hash()},
      descriptor_set_layout{CreateDescriptorSetLayout(bindings)},
      descriptor_allocator{descriptor_pool, *descriptor_set_layout},
      update_descriptor_queue{update_descriptor_queue}, layout{CreatePipelineLayout()},
      descriptor_template{CreateDescriptorUpdateTemplate(program)}, modules{CreateShaderModules(
                                                                        program)},
      renderpass{renderpass_cache.GetRenderPass(key.renderpass_params)}, pipeline{CreatePipeline(
                                                                             key.renderpass_params,
                                                                             program)} {}

VKGraphicsPipeline::~VKGraphicsPipeline() = default;

vk::DescriptorSet VKGraphicsPipeline::CommitDescriptorSet() {
    if (!descriptor_template) {
        return {};
    }
    const auto set = descriptor_allocator.Commit(scheduler.GetFence());
    update_descriptor_queue.Send(*descriptor_template, set);
    return set;
}

UniqueDescriptorSetLayout VKGraphicsPipeline::CreateDescriptorSetLayout(
    const std::vector<vk::DescriptorSetLayoutBinding>& bindings) const {
    const vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_ci(
        {}, static_cast<u32>(bindings.size()), bindings.data());

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createDescriptorSetLayoutUnique(descriptor_set_layout_ci, nullptr, dld);
}

UniquePipelineLayout VKGraphicsPipeline::CreatePipelineLayout() const {
    const vk::PipelineLayoutCreateInfo pipeline_layout_ci({}, 1, &*descriptor_set_layout, 0,
                                                          nullptr);
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createPipelineLayoutUnique(pipeline_layout_ci, nullptr, dld);
}

UniqueDescriptorUpdateTemplate VKGraphicsPipeline::CreateDescriptorUpdateTemplate(
    const SPIRVProgram& program) const {
    std::vector<vk::DescriptorUpdateTemplateEntry> template_entries;
    u32 binding = 0;
    u32 offset = 0;
    for (const auto& stage : program) {
        if (stage) {
            FillDescriptorUpdateTemplateEntries(device, stage->entries, binding, offset,
                                                template_entries);
        }
    }
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

std::vector<UniqueShaderModule> VKGraphicsPipeline::CreateShaderModules(
    const SPIRVProgram& program) const {
    std::vector<UniqueShaderModule> modules;
    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    for (std::size_t i = 0; i < Maxwell::MaxShaderStage; ++i) {
        const auto& stage = program[i];
        if (!stage) {
            continue;
        }
        const vk::ShaderModuleCreateInfo module_ci({}, stage->code.size() * sizeof(u32),
                                                   stage->code.data());
        modules.emplace_back(dev.createShaderModuleUnique(module_ci, nullptr, dld));
    }
    return modules;
}

UniquePipeline VKGraphicsPipeline::CreatePipeline(const RenderPassParams& renderpass_params,
                                                  const SPIRVProgram& program) const {
    const auto& vi = fixed_state.vertex_input;
    const auto& ia = fixed_state.input_assembly;
    const auto& ds = fixed_state.depth_stencil;
    const auto& cd = fixed_state.color_blending;
    const auto& ts = fixed_state.tessellation;
    const auto& rs = fixed_state.rasterizer;

    std::vector<vk::VertexInputBindingDescription> vertex_bindings;
    std::vector<vk::VertexInputBindingDivisorDescriptionEXT> vertex_binding_divisors;
    for (std::size_t i = 0; i < vi.num_bindings; ++i) {
        const auto& binding = vi.bindings[i];
        const bool instanced = binding.divisor != 0;
        const auto rate = instanced ? vk::VertexInputRate::eInstance : vk::VertexInputRate::eVertex;
        vertex_bindings.emplace_back(binding.index, binding.stride, rate);
        if (instanced) {
            vertex_binding_divisors.emplace_back(binding.index, binding.divisor);
        }
    }

    std::vector<vk::VertexInputAttributeDescription> vertex_attributes;
    const auto& input_attributes = program[0]->entries.attributes;
    for (std::size_t i = 0; i < vi.num_attributes; ++i) {
        const auto& attribute = vi.attributes[i];
        if (input_attributes.find(attribute.index) == input_attributes.end()) {
            // Skip attributes not used by the vertex shaders.
            continue;
        }
        vertex_attributes.emplace_back(attribute.index, attribute.buffer,
                                       MaxwellToVK::VertexFormat(attribute.type, attribute.size),
                                       attribute.offset);
    }

    vk::PipelineVertexInputStateCreateInfo vertex_input_ci(
        {}, static_cast<u32>(vertex_bindings.size()), vertex_bindings.data(),
        static_cast<u32>(vertex_attributes.size()), vertex_attributes.data());

    const vk::PipelineVertexInputDivisorStateCreateInfoEXT vertex_input_divisor_ci(
        static_cast<u32>(vertex_binding_divisors.size()), vertex_binding_divisors.data());
    if (!vertex_binding_divisors.empty()) {
        vertex_input_ci.pNext = &vertex_input_divisor_ci;
    }

    const auto primitive_topology = MaxwellToVK::PrimitiveTopology(device, ia.topology);
    const vk::PipelineInputAssemblyStateCreateInfo input_assembly_ci(
        {}, primitive_topology,
        ia.primitive_restart_enable && SupportsPrimitiveRestart(primitive_topology));

    const vk::PipelineTessellationStateCreateInfo tessellation_ci({}, ts.patch_control_points);

    const vk::PipelineViewportStateCreateInfo viewport_ci({}, Maxwell::NumViewports, nullptr,
                                                          Maxwell::NumViewports, nullptr);

    // TODO(Rodrigo): Find out what's the default register value for front face
    const vk::PipelineRasterizationStateCreateInfo rasterizer_ci(
        {}, rs.depth_clamp_enable, false, vk::PolygonMode::eFill,
        rs.cull_enable ? MaxwellToVK::CullFace(rs.cull_face) : vk::CullModeFlagBits::eNone,
        rs.cull_enable ? MaxwellToVK::FrontFace(rs.front_face) : vk::FrontFace::eCounterClockwise,
        rs.depth_bias_enable, 0.0f, 0.0f, 0.0f, 1.0f);

    const vk::PipelineMultisampleStateCreateInfo multisampling_ci(
        {}, vk::SampleCountFlagBits::e1, false, 0.0f, nullptr, false, false);

    const vk::CompareOp depth_test_compare = ds.depth_test_enable
                                                 ? MaxwellToVK::ComparisonOp(ds.depth_test_function)
                                                 : vk::CompareOp::eAlways;

    const vk::PipelineDepthStencilStateCreateInfo depth_stencil_ci(
        {}, ds.depth_test_enable, ds.depth_write_enable, depth_test_compare, ds.depth_bounds_enable,
        ds.stencil_enable, GetStencilFaceState(ds.front_stencil),
        GetStencilFaceState(ds.back_stencil), 0.0f, 0.0f);

    std::array<vk::PipelineColorBlendAttachmentState, Maxwell::NumRenderTargets> cb_attachments;
    const std::size_t num_attachments =
        std::min(cd.attachments_count, renderpass_params.color_attachments.size());
    for (std::size_t i = 0; i < num_attachments; ++i) {
        constexpr std::array component_table{
            vk::ColorComponentFlagBits::eR, vk::ColorComponentFlagBits::eG,
            vk::ColorComponentFlagBits::eB, vk::ColorComponentFlagBits::eA};
        const auto& blend = cd.attachments[i];

        vk::ColorComponentFlags color_components{};
        for (std::size_t j = 0; j < component_table.size(); ++j) {
            if (blend.components[j])
                color_components |= component_table[j];
        }

        cb_attachments[i] = vk::PipelineColorBlendAttachmentState(
            blend.enable, MaxwellToVK::BlendFactor(blend.src_rgb_func),
            MaxwellToVK::BlendFactor(blend.dst_rgb_func),
            MaxwellToVK::BlendEquation(blend.rgb_equation),
            MaxwellToVK::BlendFactor(blend.src_a_func), MaxwellToVK::BlendFactor(blend.dst_a_func),
            MaxwellToVK::BlendEquation(blend.a_equation), color_components);
    }
    const vk::PipelineColorBlendStateCreateInfo color_blending_ci({}, false, vk::LogicOp::eCopy,
                                                                  static_cast<u32>(num_attachments),
                                                                  cb_attachments.data(), {});

    constexpr std::array dynamic_states = {
        vk::DynamicState::eViewport,         vk::DynamicState::eScissor,
        vk::DynamicState::eDepthBias,        vk::DynamicState::eBlendConstants,
        vk::DynamicState::eDepthBounds,      vk::DynamicState::eStencilCompareMask,
        vk::DynamicState::eStencilWriteMask, vk::DynamicState::eStencilReference};
    const vk::PipelineDynamicStateCreateInfo dynamic_state_ci(
        {}, static_cast<u32>(dynamic_states.size()), dynamic_states.data());

    vk::PipelineShaderStageRequiredSubgroupSizeCreateInfoEXT subgroup_size_ci;
    subgroup_size_ci.requiredSubgroupSize = GuestWarpSize;

    std::vector<vk::PipelineShaderStageCreateInfo> shader_stages;
    std::size_t module_index = 0;
    for (std::size_t stage = 0; stage < Maxwell::MaxShaderStage; ++stage) {
        if (!program[stage]) {
            continue;
        }
        const auto stage_enum = static_cast<Tegra::Engines::ShaderType>(stage);
        const auto vk_stage = MaxwellToVK::ShaderStage(stage_enum);
        auto& stage_ci = shader_stages.emplace_back(vk::PipelineShaderStageCreateFlags{}, vk_stage,
                                                    *modules[module_index++], "main", nullptr);
        if (program[stage]->entries.uses_warps && device.IsGuestWarpSizeSupported(vk_stage)) {
            stage_ci.pNext = &subgroup_size_ci;
        }
    }

    const vk::GraphicsPipelineCreateInfo create_info(
        {}, static_cast<u32>(shader_stages.size()), shader_stages.data(), &vertex_input_ci,
        &input_assembly_ci, &tessellation_ci, &viewport_ci, &rasterizer_ci, &multisampling_ci,
        &depth_stencil_ci, &color_blending_ci, &dynamic_state_ci, *layout, renderpass, 0, {}, 0);

    const auto dev = device.GetLogical();
    const auto& dld = device.GetDispatchLoader();
    return dev.createGraphicsPipelineUnique(nullptr, create_info, nullptr, dld);
}

} // namespace Vulkan
