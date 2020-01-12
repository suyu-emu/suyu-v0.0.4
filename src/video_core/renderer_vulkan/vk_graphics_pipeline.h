// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/fixed_pipeline_state.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_renderpass_cache.h"
#include "video_core/renderer_vulkan/vk_resource_manager.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"

namespace Vulkan {

using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct GraphicsPipelineCacheKey;

class VKDescriptorPool;
class VKDevice;
class VKRenderPassCache;
class VKScheduler;
class VKUpdateDescriptorQueue;

using SPIRVProgram = std::array<std::optional<SPIRVShader>, Maxwell::MaxShaderStage>;

class VKGraphicsPipeline final {
public:
    explicit VKGraphicsPipeline(const VKDevice& device, VKScheduler& scheduler,
                                VKDescriptorPool& descriptor_pool,
                                VKUpdateDescriptorQueue& update_descriptor_queue,
                                VKRenderPassCache& renderpass_cache,
                                const GraphicsPipelineCacheKey& key,
                                const std::vector<vk::DescriptorSetLayoutBinding>& bindings,
                                const SPIRVProgram& program);
    ~VKGraphicsPipeline();

    vk::DescriptorSet CommitDescriptorSet();

    vk::Pipeline GetHandle() const {
        return *pipeline;
    }

    vk::PipelineLayout GetLayout() const {
        return *layout;
    }

    vk::RenderPass GetRenderPass() const {
        return renderpass;
    }

private:
    UniqueDescriptorSetLayout CreateDescriptorSetLayout(
        const std::vector<vk::DescriptorSetLayoutBinding>& bindings) const;

    UniquePipelineLayout CreatePipelineLayout() const;

    UniqueDescriptorUpdateTemplate CreateDescriptorUpdateTemplate(
        const SPIRVProgram& program) const;

    std::vector<UniqueShaderModule> CreateShaderModules(const SPIRVProgram& program) const;

    UniquePipeline CreatePipeline(const RenderPassParams& renderpass_params,
                                  const SPIRVProgram& program) const;

    const VKDevice& device;
    VKScheduler& scheduler;
    const FixedPipelineState fixed_state;
    const u64 hash;

    UniqueDescriptorSetLayout descriptor_set_layout;
    DescriptorAllocator descriptor_allocator;
    VKUpdateDescriptorQueue& update_descriptor_queue;
    UniquePipelineLayout layout;
    UniqueDescriptorUpdateTemplate descriptor_template;
    std::vector<UniqueShaderModule> modules;

    vk::RenderPass renderpass;
    UniquePipeline pipeline;
};

} // namespace Vulkan
