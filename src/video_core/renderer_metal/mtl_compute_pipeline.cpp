// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <vector>

#include <boost/container/small_vector.hpp>

#include "video_core/buffer_cache/buffer_cache_base.h"
#include "video_core/renderer_metal/mtl_compute_pipeline.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/shader_notify.h"
#include "video_core/texture_cache/texture_cache_base.h"

namespace Metal {

ComputePipeline::ComputePipeline(const Device& device_, VideoCore::ShaderNotify* shader_notify,
                                 const Shader::Info& info_, MTL::Function* function_)
    : device{device_}, info{info_}, function{function_} {
    if (shader_notify) {
        shader_notify->MarkShaderBuilding();
    }

    MTL::ComputePipelineDescriptor* pipeline_descriptor =
        MTL::ComputePipelineDescriptor::alloc()->init();
    pipeline_descriptor->setComputeFunction(function);
    // TODO: set other properties

    NS::Error* error = nullptr;
    pipeline_state =
        device.GetDevice()->newComputePipelineState(pipeline_descriptor, 0, nullptr, &error);
    if (error) {
        LOG_ERROR(Render_Metal, "failed to create compute pipeline: {}",
                  error->description()->cString(NS::ASCIIStringEncoding));
    }
}

void ComputePipeline::Configure(Tegra::Engines::KeplerCompute& kepler_compute,
                                Tegra::MemoryManager& gpu_memory, CommandRecorder& scheduler,
                                BufferCache& buffer_cache, TextureCache& texture_cache) {
    // TODO: bind resources
}

} // namespace Metal
