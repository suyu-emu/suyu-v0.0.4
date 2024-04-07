// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <memory>
#include <thread>
#include <vector>

#include "common/bit_cast.h"
#include "common/cityhash.h"
#include "common/fs/fs.h"
#include "common/fs/path_util.h"
#include "common/microprofile.h"
#include "common/thread_worker.h"
#include "core/core.h"
#include "shader_recompiler/backend/spirv/emit_spirv.h"
#include "shader_recompiler/environment.h"
#include "shader_recompiler/frontend/maxwell/control_flow.h"
#include "shader_recompiler/frontend/maxwell/translate_program.h"
#include "shader_recompiler/program_header.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/memory_manager.h"
#include "video_core/renderer_metal/mtl_compute_pipeline.h"
#include "video_core/renderer_metal/mtl_device.h"
#include "video_core/renderer_metal/mtl_pipeline_cache.h"
#include "video_core/shader_cache.h"
#include "video_core/shader_environment.h"
#include "video_core/shader_notify.h"

namespace Metal {

namespace {
using Shader::Backend::SPIRV::EmitSPIRV;
using Shader::Maxwell::ConvertLegacyToGeneric;
using Shader::Maxwell::GenerateGeometryPassthrough;
using Shader::Maxwell::MergeDualVertexPrograms;
using Shader::Maxwell::TranslateProgram;
using VideoCommon::ComputeEnvironment;
using VideoCommon::FileEnvironment;
using VideoCommon::GenericEnvironment;
using VideoCommon::GraphicsEnvironment;

// constexpr u32 CACHE_VERSION = 1;
// constexpr std::array<char, 8> METAL_CACHE_MAGIC_NUMBER{'s', 'u', 'y', 'u', 'm', 'l', 'c', 'h'};

template <typename Container>
auto MakeSpan(Container& container) {
    return std::span(container.data(), container.size());
}

} // Anonymous namespace

size_t ComputePipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), sizeof *this);
    return static_cast<size_t>(hash);
}

bool ComputePipelineCacheKey::operator==(const ComputePipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, sizeof *this) == 0;
}

size_t GraphicsPipelineCacheKey::Hash() const noexcept {
    const u64 hash = Common::CityHash64(reinterpret_cast<const char*>(this), Size());
    return static_cast<size_t>(hash);
}

bool GraphicsPipelineCacheKey::operator==(const GraphicsPipelineCacheKey& rhs) const noexcept {
    return std::memcmp(&rhs, this, Size()) == 0;
}

PipelineCache::PipelineCache(Tegra::MaxwellDeviceMemoryManager& device_memory_,
                             const Device& device_, CommandRecorder& command_recorder_,
                             BufferCache& buffer_cache_, TextureCache& texture_cache_,
                             VideoCore::ShaderNotify& shader_notify_)
    : VideoCommon::ShaderCache{device_memory_}, device{device_},
      command_recorder{command_recorder_}, buffer_cache{buffer_cache_},
      texture_cache{texture_cache_}, shader_notify{shader_notify_} {
    // TODO: query for some of these parameters
    profile = Shader::Profile{
        .supported_spirv = 0x00010300U, // HACK
        .unified_descriptor_binding = false,
        .support_descriptor_aliasing = false,
        .support_int8 = true,
        .support_int16 = true,
        .support_int64 = true,
        .support_vertex_instance_id = false,
        .support_float_controls = false,
        .support_separate_denorm_behavior = false,
        .support_separate_rounding_mode = false,
        .support_fp16_denorm_preserve = false,
        .support_fp32_denorm_preserve = false,
        .support_fp16_denorm_flush = false,
        .support_fp32_denorm_flush = false,
        .support_fp16_signed_zero_nan_preserve = false,
        .support_fp32_signed_zero_nan_preserve = false,
        .support_fp64_signed_zero_nan_preserve = false,
        .support_explicit_workgroup_layout = false,
        .support_vote = false,
        .support_viewport_index_layer_non_geometry = false,
        .support_viewport_mask = false,
        .support_typeless_image_loads = true,
        .support_demote_to_helper_invocation = false,
        .support_int64_atomics = false,
        .support_derivative_control = true,
        .support_geometry_shader_passthrough = false,
        .support_native_ndc = false,
        .support_scaled_attributes = false,
        .support_multi_viewport = false,
        .support_geometry_streams = false,

        .warp_size_potentially_larger_than_guest = false,

        .lower_left_origin_mode = false,
        .need_declared_frag_colors = false,
        .need_gather_subpixel_offset = false,

        .has_broken_spirv_clamp = false,
        .has_broken_spirv_position_input = false,
        .has_broken_unsigned_image_offsets = false,
        .has_broken_signed_operations = false,
        .has_broken_fp16_float_controls = false,
        .ignore_nan_fp_comparisons = false,
        .has_broken_spirv_subgroup_mask_vector_extract_dynamic = false,
        .has_broken_robust = false,
        .min_ssbo_alignment = 4,
        .max_user_clip_distances = 8,
    };

    host_info = Shader::HostTranslateInfo{
        .support_float64 = false,
        .support_float16 = true,
        .support_int64 = false,
        .needs_demote_reorder = false,
        .support_snorm_render_buffer = true,
        .support_viewport_index_layer = true,
        .min_ssbo_alignment = 4,
        .support_geometry_shader_passthrough = false,
        .support_conditional_barrier = false,
    };
}

PipelineCache::~PipelineCache() = default;

GraphicsPipeline* PipelineCache::CurrentGraphicsPipeline() {
    if (!RefreshStages(graphics_key.unique_hashes)) {
        current_pipeline = nullptr;
        return nullptr;
    }

    if (current_pipeline) {
        GraphicsPipeline* const next{current_pipeline->Next(graphics_key)};
        if (next) {
            current_pipeline = next;
            return BuiltPipeline(current_pipeline);
        }
    }

    return CurrentGraphicsPipelineSlowPath();
}

ComputePipeline* PipelineCache::CurrentComputePipeline() {
    const ShaderInfo* const shader{ComputeShader()};
    if (!shader) {
        return nullptr;
    }
    const auto& qmd{kepler_compute->launch_description};
    const ComputePipelineCacheKey key{
        .unique_hash = shader->unique_hash,
        .shared_memory_size = qmd.shared_alloc,
        .threadgroup_size{qmd.block_dim_x, qmd.block_dim_y, qmd.block_dim_z},
    };
    const auto [pair, is_new]{compute_cache.try_emplace(key)};
    auto& pipeline{pair->second};
    if (!is_new) {
        return pipeline.get();
    }
    pipeline = CreateComputePipeline(key, shader);

    return pipeline.get();
}

void PipelineCache::LoadDiskResources(u64 title_id, std::stop_token stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback) {
    // TODO: implement
}

GraphicsPipeline* PipelineCache::CurrentGraphicsPipelineSlowPath() {
    const auto [pair, is_new]{graphics_cache.try_emplace(graphics_key)};
    auto& pipeline{pair->second};
    if (is_new) {
        pipeline = CreateGraphicsPipeline();
    }
    if (!pipeline) {
        return nullptr;
    }
    current_pipeline = pipeline.get();

    return BuiltPipeline(current_pipeline);
}

GraphicsPipeline* PipelineCache::BuiltPipeline(GraphicsPipeline* pipeline) const noexcept {
    if (pipeline->IsBuilt()) {
        return pipeline;
    }
    const auto& draw_state = maxwell3d->draw_manager->GetDrawState();
    if (draw_state.index_buffer.count <= 6 || draw_state.vertex_buffer.count <= 6) {
        return pipeline;
    }

    return nullptr;
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline(
    ShaderPools& pools, const GraphicsPipelineCacheKey& key,
    std::span<Shader::Environment* const> envs) try {
    auto hash = key.Hash();
    LOG_INFO(Render_Metal, "0x{:016x}", hash);

    // HACK: create hardcoded shaders
    MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc()->init();
    NS::Error* error = nullptr;
    MTL::Library* library = device.GetDevice()->newLibrary(NS::String::string(
                                                               R"(
        #include <metal_stdlib>
        using namespace metal;

        constant float2 texCoords[] = {
            float2(0.0, -1.0),
            float2(0.0,  1.0),
            float2(2.0,  1.0),
        };

        struct VertexOut {
            float4 position [[position]];
            float2 texCoord;
        };

        vertex VertexOut vertexMain(uint vid [[vertex_id]]) {
            VertexOut out;
            out.position = float4(texCoords[vid] * 2.0 - 1.0, 0.0, 1.0);
            out.texCoord = texCoords[vid];

            return out;
        }

        fragment float4 fragmentMain(VertexOut in [[stage_in]]) {
            return float4(in.texCoord, 0.0, 1.0);
        }
    )",
                                                               NS::ASCIIStringEncoding),
                                                           compile_options, &error);
    if (error) {
        LOG_ERROR(Render_Metal, "failed to create blit library: {}",
                  error->description()->cString(NS::ASCIIStringEncoding));
    }

    std::array<MTL::Function*, VideoCommon::NUM_STAGES> functions;

    functions[0] = library->newFunction(NS::String::string("vertexMain", NS::ASCIIStringEncoding));
    functions[1] =
        library->newFunction(NS::String::string("fragmentMain", NS::ASCIIStringEncoding));

    // HACK: dummy info
    std::array<const Shader::Info*, VideoCommon::NUM_STAGES> infos = {nullptr};
    infos[0] = new Shader::Info{};
    infos[1] = new Shader::Info{};

    return std::make_unique<GraphicsPipeline>(device, command_recorder, key, buffer_cache,
                                              texture_cache, &shader_notify, functions, infos);
} catch (const std::exception& e) {
    LOG_ERROR(Render_Metal, "failed to create graphics pipeline: {}", e.what());
    return nullptr;
}

std::unique_ptr<GraphicsPipeline> PipelineCache::CreateGraphicsPipeline() {
    GraphicsEnvironments environments;
    GetGraphicsEnvironments(environments, graphics_key.unique_hashes);

    main_pools.ReleaseContents();

    return CreateGraphicsPipeline(main_pools, graphics_key, environments.Span());
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    const ComputePipelineCacheKey& key, const ShaderInfo* shader) {
    const GPUVAddr program_base{kepler_compute->regs.code_loc.Address()};
    const auto& qmd{kepler_compute->launch_description};
    ComputeEnvironment env{*kepler_compute, *gpu_memory, program_base, qmd.program_start};
    env.SetCachedSize(shader->size_bytes);

    main_pools.ReleaseContents();

    return CreateComputePipeline(main_pools, key, env);
}

std::unique_ptr<ComputePipeline> PipelineCache::CreateComputePipeline(
    ShaderPools& pools, const ComputePipelineCacheKey& key, Shader::Environment& env) try {
    auto hash = key.Hash();
    LOG_INFO(Render_Metal, "0x{:016x}", hash);

    MTL::Function* function = nullptr;
    // TODO: create compute function

    throw std::runtime_error("Compute shaders are not implemented");

    return std::make_unique<ComputePipeline>(device, &shader_notify, Shader::Info{}, function);
} catch (const std::exception& e) {
    LOG_ERROR(Render_Metal, "failed to create compute pipeline: {}", e.what());
    return nullptr;
}

} // namespace Metal
