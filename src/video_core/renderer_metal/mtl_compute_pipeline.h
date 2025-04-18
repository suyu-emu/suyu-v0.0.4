// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

#include "common/common_types.h"
#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/renderer_metal/mtl_buffer_cache.h"
#include "video_core/renderer_metal/mtl_texture_cache.h"

namespace VideoCore {
class ShaderNotify;
}

namespace Metal {

class Device;
class PipelineStatistics;
class Scheduler;

class ComputePipeline {
public:
    explicit ComputePipeline(const Device& device_, VideoCore::ShaderNotify* shader_notify,
                             const Shader::Info& info_, MTL::Function* function_);

    ComputePipeline& operator=(ComputePipeline&&) noexcept = delete;
    ComputePipeline(ComputePipeline&&) noexcept = delete;

    ComputePipeline& operator=(const ComputePipeline&) = delete;
    ComputePipeline(const ComputePipeline&) = delete;

    void Configure(Tegra::Engines::KeplerCompute& kepler_compute, Tegra::MemoryManager& gpu_memory,
                   CommandRecorder& scheduler, BufferCache& buffer_cache,
                   TextureCache& texture_cache);

private:
    const Device& device;
    Shader::Info info;

    MTL::Function* function;
    MTL::ComputePipelineState* pipeline_state;
};

} // namespace Metal
