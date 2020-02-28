// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <tuple>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/shader/registry.h"

namespace VideoCommon::Shader {

using Tegra::Engines::SamplerDescriptor;

Registry::Registry(Tegra::Engines::ShaderType shader_stage,
                   VideoCore::GuestDriverProfile stored_guest_driver_profile)
    : stage{shader_stage}, stored_guest_driver_profile{stored_guest_driver_profile} {}

Registry::Registry(Tegra::Engines::ShaderType shader_stage,
                   Tegra::Engines::ConstBufferEngineInterface& engine)
    : stage{shader_stage}, engine{&engine} {}

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

std::optional<u32> Registry::ObtainBoundBuffer() {
    if (bound_buffer_saved) {
        return bound_buffer;
    }
    if (!engine) {
        return std::nullopt;
    }
    bound_buffer_saved = true;
    bound_buffer = engine->GetBoundBuffer();
    return bound_buffer;
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

void Registry::SetBoundBuffer(u32 buffer) {
    bound_buffer_saved = true;
    bound_buffer = buffer;
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

} // namespace VideoCommon::Shader
