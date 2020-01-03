// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <tuple>

#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/engines/shader_type.h"
#include "video_core/shader/const_buffer_locker.h"

namespace VideoCommon::Shader {

using Tegra::Engines::SamplerDescriptor;

ConstBufferLocker::ConstBufferLocker(Tegra::Engines::ShaderType shader_stage)
    : stage{shader_stage} {}

ConstBufferLocker::ConstBufferLocker(Tegra::Engines::ShaderType shader_stage,
                                     Tegra::Engines::ConstBufferEngineInterface& engine)
    : stage{shader_stage}, engine{&engine} {}

ConstBufferLocker::~ConstBufferLocker() = default;

std::optional<u32> ConstBufferLocker::ObtainKey(u32 buffer, u32 offset) {
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

std::optional<SamplerDescriptor> ConstBufferLocker::ObtainBoundSampler(u32 offset) {
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

std::optional<Tegra::Engines::SamplerDescriptor> ConstBufferLocker::ObtainBindlessSampler(
    u32 buffer, u32 offset) {
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

std::optional<u32> ConstBufferLocker::ObtainBoundBuffer() {
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

void ConstBufferLocker::InsertKey(u32 buffer, u32 offset, u32 value) {
    keys.insert_or_assign({buffer, offset}, value);
}

void ConstBufferLocker::InsertBoundSampler(u32 offset, SamplerDescriptor sampler) {
    bound_samplers.insert_or_assign(offset, sampler);
}

void ConstBufferLocker::InsertBindlessSampler(u32 buffer, u32 offset, SamplerDescriptor sampler) {
    bindless_samplers.insert_or_assign({buffer, offset}, sampler);
}

void ConstBufferLocker::SetBoundBuffer(u32 buffer) {
    bound_buffer_saved = true;
    bound_buffer = buffer;
}

bool ConstBufferLocker::IsConsistent() const {
    if (!engine) {
        return false;
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

bool ConstBufferLocker::HasEqualKeys(const ConstBufferLocker& rhs) const {
    return std::tie(keys, bound_samplers, bindless_samplers) ==
           std::tie(rhs.keys, rhs.bound_samplers, rhs.bindless_samplers);
}

} // namespace VideoCommon::Shader
