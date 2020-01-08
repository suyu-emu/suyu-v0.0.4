// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "video_core/renderer_vulkan/declarations.h"
#include "video_core/renderer_vulkan/vk_descriptor_pool.h"
#include "video_core/renderer_vulkan/vk_shader_decompiler.h"

namespace Vulkan {

class VKDevice;
class VKScheduler;
class VKUpdateDescriptorQueue;

class VKComputePipeline final {
public:
    explicit VKComputePipeline(const VKDevice& device, VKScheduler& scheduler,
                               VKDescriptorPool& descriptor_pool,
                               VKUpdateDescriptorQueue& update_descriptor_queue,
                               const SPIRVShader& shader);
    ~VKComputePipeline();

    vk::DescriptorSet CommitDescriptorSet();

    vk::Pipeline GetHandle() const {
        return *pipeline;
    }

    vk::PipelineLayout GetLayout() const {
        return *layout;
    }

    const ShaderEntries& GetEntries() {
        return entries;
    }

private:
    UniqueDescriptorSetLayout CreateDescriptorSetLayout() const;

    UniquePipelineLayout CreatePipelineLayout() const;

    UniqueDescriptorUpdateTemplate CreateDescriptorUpdateTemplate() const;

    UniqueShaderModule CreateShaderModule(const std::vector<u32>& code) const;

    UniquePipeline CreatePipeline() const;

    const VKDevice& device;
    VKScheduler& scheduler;
    ShaderEntries entries;

    UniqueDescriptorSetLayout descriptor_set_layout;
    DescriptorAllocator descriptor_allocator;
    VKUpdateDescriptorQueue& update_descriptor_queue;
    UniquePipelineLayout layout;
    UniqueDescriptorUpdateTemplate descriptor_template;
    UniqueShaderModule shader_module;
    UniquePipeline pipeline;
};

} // namespace Vulkan
