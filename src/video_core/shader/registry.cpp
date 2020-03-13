// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <tuple>

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/kepler_compute.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/shader/registry.h"

namespace VideoCommon::Shader {

using Tegra::Engines::ConstBufferEngineInterface;
using Tegra::Engines::SamplerDescriptor;
using Tegra::Engines::ShaderType;

namespace {

GraphicsInfo MakeGraphicsInfo(ShaderType shader_stage, ConstBufferEngineInterface& engine) {
    if (shader_stage == ShaderType::Compute) {
        return {};
    }
    auto& graphics = static_cast<Tegra::Engines::Maxwell3D&>(engine);

    GraphicsInfo info;
    info.tfb_layouts = graphics.regs.tfb_layouts;
    info.tfb_varying_locs = graphics.regs.tfb_varying_locs;
    info.primitive_topology = graphics.regs.draw.topology;
    info.tessellation_primitive = graphics.regs.tess_mode.prim;
    info.tessellation_spacing = graphics.regs.tess_mode.spacing;
    info.tfb_enabled = graphics.regs.tfb_enabled;
    info.tessellation_clockwise = graphics.regs.tess_mode.cw;
    return info;
}

ComputeInfo MakeComputeInfo(ShaderType shader_stage, ConstBufferEngineInterface& engine) {
    if (shader_stage != ShaderType::Compute) {
        return {};
    }
    auto& compute = static_cast<Tegra::Engines::KeplerCompute&>(engine);
    const auto& launch = compute.launch_description;

    ComputeInfo info;
    info.workgroup_size = {launch.block_dim_x, launch.block_dim_y, launch.block_dim_z};
    info.local_memory_size_in_words = launch.local_pos_alloc;
    info.shared_memory_size_in_words = launch.shared_alloc;
    return info;
}

} // Anonymous namespace

Registry::Registry(Tegra::Engines::ShaderType shader_stage, const SerializedRegistryInfo& info)
    : stage{shader_stage}, stored_guest_driver_profile{info.guest_driver_profile},
      bound_buffer{info.bound_buffer}, graphics_info{info.graphics}, compute_info{info.compute} {}

Registry::Registry(Tegra::Engines::ShaderType shader_stage,
                   Tegra::Engines::ConstBufferEngineInterface& engine)
    : stage{shader_stage}, engine{&engine}, bound_buffer{engine.GetBoundBuffer()},
      graphics_info{MakeGraphicsInfo(shader_stage, engine)}, compute_info{MakeComputeInfo(
                                                                 shader_stage, engine)} {}

Registry::~Registry() = default;

std::optional<u32> Registry::ObtainKey(u32 buffer, u32 offset) {
    const std::pair<u32, u32> key = {buffer, offset};
    const auto iter = keys.find(key);
    if (iter != keys.end()) {
        return iter->second;
    }
    if (!engine) {
        return std::nullopt;
    }
    const u32 value = engine->AccessConstBuffer32(stage, buffer, offset);
    keys.emplace(key, value);
    return value;
}

std::optional<SamplerDescriptor> Registry::ObtainBoundSampler(u32 offset) {
    const u32 key = offset;
    const auto iter = bound_samplers.find(key);
    if (iter != bound_samplers.end()) {
        return iter->second;
    }
    if (!engine) {
        return std::nullopt;
    }
    const SamplerDescriptor value = engine->AccessBoundSampler(stage, offset);
    bound_samplers.emplace(key, value);
    return value;
}

std::optional<Tegra::Engines::SamplerDescriptor> Registry::ObtainBindlessSampler(u32 buffer,
                                                                                 u32 offset) {
    const std::pair key = {buffer, offset};
    const auto iter = bindless_samplers.find(key);
    if (iter != bindless_samplers.end()) {
        return iter->second;
    }
    if (!engine) {
        return std::nullopt;
    }
    const SamplerDescriptor value = engine->AccessBindlessSampler(stage, buffer, offset);
    bindless_samplers.emplace(key, value);
    return value;
}

void Registry::InsertKey(u32 buffer, u32 offset, u32 value) {
    keys.insert_or_assign({buffer, offset}, value);
}

void Registry::InsertBoundSampler(u32 offset, SamplerDescriptor sampler) {
    bound_samplers.insert_or_assign(offset, sampler);
}

void Registry::InsertBindlessSampler(u32 buffer, u32 offset, SamplerDescriptor sampler) {
    bindless_samplers.insert_or_assign({buffer, offset}, sampler);
}

bool Registry::IsConsistent() const {
    if (!engine) {
        return true;
    }
    return std::all_of(keys.begin(), keys.end(),
                       [this](const auto& pair) {
                           const auto [cbuf, offset] = pair.first;
                           const auto value = pair.second;
                           return value == engine->AccessConstBuffer32(stage, cbuf, offset);
                       }) &&
           std::all_of(bound_samplers.begin(), bound_samplers.end(),
                       [this](const auto& sampler) {
                           const auto [key, value] = sampler;
                           return value == engine->AccessBoundSampler(stage, key);
                       }) &&
           std::all_of(bindless_samplers.begin(), bindless_samplers.end(),
                       [this](const auto& sampler) {
                           const auto [cbuf, offset] = sampler.first;
                           const auto value = sampler.second;
                           return value == engine->AccessBindlessSampler(stage, cbuf, offset);
                       });
}

bool Registry::HasEqualKeys(const Registry& rhs) const {
    return std::tie(keys, bound_samplers, bindless_samplers) ==
           std::tie(rhs.keys, rhs.bound_samplers, rhs.bindless_samplers);
}

const GraphicsInfo& Registry::GetGraphicsInfo() const {
    ASSERT(stage != Tegra::Engines::ShaderType::Compute);
    return graphics_info;
}

const ComputeInfo& Registry::GetComputeInfo() const {
    ASSERT(stage == Tegra::Engines::ShaderType::Compute);
    return compute_info;
}

} // namespace VideoCommon::Shader
