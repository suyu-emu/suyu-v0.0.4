// SPDX-FileCopyrightText: Copyright 2024 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
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
#include "shader_recompiler/backend/msl/emit_msl.h"
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
using Shader::Backend::MSL::EmitMSL;
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

Shader::RuntimeInfo MakeRuntimeInfo(std::span<const Shader::IR::Program> programs,
                                    const GraphicsPipelineCacheKey& key,
                                    const Shader::IR::Program& program,
                                    const Shader::IR::Program* previous_program) {
    Shader::RuntimeInfo info;
    if (previous_program) {
        info.previous_stage_stores = previous_program->info.stores;
        info.previous_stage_legacy_stores_mapping = previous_program->info.legacy_stores_mapping;
        if (previous_program->is_geometry_passthrough) {
            info.previous_stage_stores.mask |= previous_program->info.passthrough.mask;
        }
    } else {
        info.previous_stage_stores.mask.set();
    }
    // TODO: uncomment
    /*
    const Shader::Stage stage{program.stage};
    const bool has_geometry{key.unique_hashes[4] != 0 && !programs[4].is_geometry_passthrough};
    const bool gl_ndc{key.state.ndc_minus_one_to_one != 0};
    const float point_size{Common::BitCast<float>(key.state.point_size)};
    switch (stage) {
    case Shader::Stage::VertexB:
        if (!has_geometry) {
            if (key.state.topology == Maxwell::PrimitiveTopology::Points) {
                info.fixed_state_point_size = point_size;
            }
            if (key.state.xfb_enabled) {
                auto [varyings, count] =
                    VideoCommon::MakeTransformFeedbackVaryings(key.state.xfb_state);
                info.xfb_varyings = varyings;
                info.xfb_count = count;
            }
            info.convert_depth_mode = gl_ndc;
        }
        if (key.state.dynamic_vertex_input) {
            for (size_t index = 0; index < Maxwell::NumVertexAttributes; ++index) {
                info.generic_input_types[index] = AttributeType(key.state, index);
            }
        } else {
            std::ranges::transform(key.state.attributes, info.generic_input_types.begin(),
                                    &CastAttributeType);
        }
        break;
    case Shader::Stage::TessellationEval:
        info.tess_clockwise = key.state.tessellation_clockwise != 0;
        info.tess_primitive = [&key] {
            const u32 raw{key.state.tessellation_primitive.Value()};
            switch (static_cast<Maxwell::Tessellation::DomainType>(raw)) {
            case Maxwell::Tessellation::DomainType::Isolines:
                return Shader::TessPrimitive::Isolines;
            case Maxwell::Tessellation::DomainType::Triangles:
                return Shader::TessPrimitive::Triangles;
            case Maxwell::Tessellation::DomainType::Quads:
                return Shader::TessPrimitive::Quads;
            }
            ASSERT(false);
            return Shader::TessPrimitive::Triangles;
        }();
        info.tess_spacing = [&] {
            const u32 raw{key.state.tessellation_spacing};
            switch (static_cast<Maxwell::Tessellation::Spacing>(raw)) {
            case Maxwell::Tessellation::Spacing::Integer:
                return Shader::TessSpacing::Equal;
            case Maxwell::Tessellation::Spacing::FractionalOdd:
                return Shader::TessSpacing::FractionalOdd;
            case Maxwell::Tessellation::Spacing::FractionalEven:
                return Shader::TessSpacing::FractionalEven;
            }
            ASSERT(false);
            return Shader::TessSpacing::Equal;
        }();
        break;
    case Shader::Stage::Geometry:
        if (program.output_topology == Shader::OutputTopology::PointList) {
            info.fixed_state_point_size = point_size;
        }
        if (key.state.xfb_enabled != 0) {
            auto [varyings, count] =
                VideoCommon::MakeTransformFeedbackVaryings(key.state.xfb_state);
            info.xfb_varyings = varyings;
            info.xfb_count = count;
        }
        info.convert_depth_mode = gl_ndc;
        break;
    case Shader::Stage::Fragment:
        info.alpha_test_func = MaxwellToCompareFunction(
            key.state.UnpackComparisonOp(key.state.alpha_test_func.Value()));
        info.alpha_test_reference = Common::BitCast<float>(key.state.alpha_test_ref);
        break;
    default:
        break;
    }
    switch (key.state.topology) {
    case Maxwell::PrimitiveTopology::Points:
        info.input_topology = Shader::InputTopology::Points;
        break;
    case Maxwell::PrimitiveTopology::Lines:
    case Maxwell::PrimitiveTopology::LineLoop:
    case Maxwell::PrimitiveTopology::LineStrip:
        info.input_topology = Shader::InputTopology::Lines;
        break;
    case Maxwell::PrimitiveTopology::Triangles:
    case Maxwell::PrimitiveTopology::TriangleStrip:
    case Maxwell::PrimitiveTopology::TriangleFan:
    case Maxwell::PrimitiveTopology::Quads:
    case Maxwell::PrimitiveTopology::QuadStrip:
    case Maxwell::PrimitiveTopology::Polygon:
    case Maxwell::PrimitiveTopology::Patches:
        info.input_topology = Shader::InputTopology::Triangles;
        break;
    case Maxwell::PrimitiveTopology::LinesAdjacency:
    case Maxwell::PrimitiveTopology::LineStripAdjacency:
        info.input_topology = Shader::InputTopology::LinesAdjacency;
        break;
    case Maxwell::PrimitiveTopology::TrianglesAdjacency:
    case Maxwell::PrimitiveTopology::TriangleStripAdjacency:
        info.input_topology = Shader::InputTopology::TrianglesAdjacency;
        break;
    }
    info.force_early_z = key.state.early_z != 0;
    info.y_negate = key.state.y_negate != 0;
    */

    return info;
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

    // TODO: what
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

    // Translate shaders
    size_t env_index{0};
    std::array<Shader::IR::Program, Maxwell::MaxShaderProgram> programs;

    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }
        Shader::Environment& env{*envs[env_index]};
        ++env_index;

        const u32 cfg_offset{static_cast<u32>(env.StartAddress() + sizeof(Shader::ProgramHeader))};
        Shader::Maxwell::Flow::CFG cfg(env, pools.flow_block, cfg_offset, index == 0);
        programs[index] = TranslateProgram(pools.inst, pools.block, env, cfg, host_info);

        if (Settings::values.dump_shaders) {
            env.Dump(hash, key.unique_hashes[index]);
        }
    }
    std::array<const Shader::Info*, Maxwell::MaxShaderStage> infos{};
    std::array<MTL::Function*, VideoCommon::NUM_STAGES> functions;

    const Shader::IR::Program* previous_stage{};
    Shader::Backend::Bindings binding;
    for (size_t index = 0; index < Maxwell::MaxShaderProgram; ++index) {
        if (key.unique_hashes[index] == 0) {
            continue;
        }

        Shader::IR::Program& program{programs[index]};
        const size_t stage_index{index - 1};
        infos[stage_index] = &program.info;

        const auto runtime_info{MakeRuntimeInfo(programs, key, program, previous_stage)};
        ConvertLegacyToGeneric(program, runtime_info);
        const std::string code{EmitMSL(profile, runtime_info, program, binding)};
        // HACK
        std::cout << "SHADER INDEX: " << stage_index << std::endl;
        std::cout << code << std::endl;
        MTL::CompileOptions* compile_options = MTL::CompileOptions::alloc()->init();
        NS::Error* error = nullptr;
        MTL::Library* library = device.GetDevice()->newLibrary(
            NS::String::string(code.c_str(), NS::ASCIIStringEncoding), compile_options, &error);
        if (error) {
            LOG_ERROR(Render_Metal, "failed to create library: {}",
                      error->description()->cString(NS::ASCIIStringEncoding));
            // HACK
            std::cout << error->description()->cString(NS::ASCIIStringEncoding) << std::endl;
            // HACK
            throw;
        }

        functions[stage_index] =
            library->newFunction(NS::String::string("main_", NS::ASCIIStringEncoding));
        previous_stage = &program;
    }

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
