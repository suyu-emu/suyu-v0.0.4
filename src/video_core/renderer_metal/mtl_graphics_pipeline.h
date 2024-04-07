// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <type_traits>

#include <Metal/Metal.hpp>

#include "common/thread_worker.h"
#include "shader_recompiler/shader_info.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_metal/mtl_buffer_cache.h"
#include "video_core/renderer_metal/mtl_texture_cache.h"

namespace VideoCore {
class ShaderNotify;
}

namespace Metal {

struct GraphicsPipelineCacheKey {
    std::array<u64, 6> unique_hashes;
    // TODO: include fixed state

    size_t Hash() const noexcept;

    bool operator==(const GraphicsPipelineCacheKey& rhs) const noexcept;

    bool operator!=(const GraphicsPipelineCacheKey& rhs) const noexcept {
        return !operator==(rhs);
    }

    size_t Size() const noexcept {
        return sizeof(unique_hashes);
    }
};
static_assert(std::has_unique_object_representations_v<GraphicsPipelineCacheKey>);
static_assert(std::is_trivially_copyable_v<GraphicsPipelineCacheKey>);
static_assert(std::is_trivially_constructible_v<GraphicsPipelineCacheKey>);

} // namespace Metal

namespace std {
template <>
struct hash<Metal::GraphicsPipelineCacheKey> {
    size_t operator()(const Metal::GraphicsPipelineCacheKey& k) const noexcept {
        return k.Hash();
    }
};
} // namespace std

namespace Metal {

class Device;
class CommandRecorder;

class GraphicsPipeline {
    static constexpr size_t NUM_STAGES = Tegra::Engines::Maxwell3D::Regs::MaxShaderStage;

public:
    // TODO: accept render pass cache as an argument to provide info on color and depth attachments
    explicit GraphicsPipeline(const Device& device_, CommandRecorder& command_recorder_,
                              const GraphicsPipelineCacheKey& key_, BufferCache& buffer_cache_,
                              TextureCache& texture_cache_, VideoCore::ShaderNotify* shader_notify,
                              std::array<MTL::Function*, NUM_STAGES> functions_,
                              const std::array<const Shader::Info*, NUM_STAGES>& infos);

    GraphicsPipeline& operator=(GraphicsPipeline&&) noexcept = delete;
    GraphicsPipeline(GraphicsPipeline&&) noexcept = delete;

    GraphicsPipeline& operator=(const GraphicsPipeline&) = delete;
    GraphicsPipeline(const GraphicsPipeline&) = delete;

    // TODO: implement
    void AddTransition(GraphicsPipeline* transition) {}

    void Configure(bool is_indexed) {
        // configure_func(this, is_indexed);
    }

    [[nodiscard]] GraphicsPipeline* Next(const GraphicsPipelineCacheKey& current_key) noexcept {
        // TODO: implement
        return nullptr;
    }

    [[nodiscard]] bool IsBuilt() const noexcept {
        return true;
    }

    template <typename Spec>
    static auto MakeConfigureSpecFunc() {
        return [](GraphicsPipeline* pl, bool is_indexed) { pl->ConfigureImpl<Spec>(is_indexed); };
    }

    void SetEngine(Tegra::Engines::Maxwell3D* maxwell3d_, Tegra::MemoryManager* gpu_memory_) {
        maxwell3d = maxwell3d_;
        gpu_memory = gpu_memory_;
    }

    MTL::RenderPipelineState* GetPipelineState() const noexcept {
        return pipeline_state;
    }

private:
    template <typename Spec>
    void ConfigureImpl(bool is_indexed);

    void ConfigureDraw();

    void MakePipeline(MTL::RenderPassDescriptor* render_pass);

    void Validate();

    const Device& device;
    CommandRecorder& command_recorder;
    const GraphicsPipelineCacheKey key;
    Tegra::Engines::Maxwell3D* maxwell3d;
    Tegra::MemoryManager* gpu_memory;
    BufferCache& buffer_cache;
    TextureCache& texture_cache;

    void (*configure_func)(GraphicsPipeline*, bool){};

    std::array<MTL::Function*, NUM_STAGES> functions;

    std::array<Shader::Info, NUM_STAGES> stage_infos;
    // VideoCommon::UniformBufferSizes uniform_buffer_sizes{};
    // u32 num_textures{};

    MTL::RenderPipelineState* pipeline_state{nullptr};
};

} // namespace Metal
