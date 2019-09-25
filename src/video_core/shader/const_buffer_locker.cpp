// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/assert.h"
#include "common/common_types.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/shader/const_buffer_locker.h"

namespace VideoCommon::Shader {

ConstBufferLocker::ConstBufferLocker(Tegra::Engines::ShaderType shader_stage)
    : engine{nullptr}, shader_stage{shader_stage} {}

ConstBufferLocker::ConstBufferLocker(Tegra::Engines::ShaderType shader_stage,
                                     Tegra::Engines::ConstBufferEngineInterface* engine)
    : engine{engine}, shader_stage{shader_stage} {}

bool ConstBufferLocker::IsEngineSet() const {
    return engine != nullptr;
}

void ConstBufferLocker::SetEngine(Tegra::Engines::ConstBufferEngineInterface* engine_) {
    engine = engine_;
}

std::optional<u32> ConstBufferLocker::ObtainKey(u32 buffer, u32 offset) {
    if (!keys) {
        keys = std::make_shared<KeyMap>();
    }
    auto& key_map = *keys;
    const std::pair<u32, u32> key = {buffer, offset};
    const auto iter = key_map.find(key);
    if (iter != key_map.end()) {
        return {iter->second};
    }
    if (!IsEngineSet()) {
        return {};
    }
    const u32 value = engine->AccessConstBuffer32(shader_stage, buffer, offset);
    key_map.emplace(key, value);
    return {value};
}

std::optional<Tegra::Engines::SamplerDescriptor> ConstBufferLocker::ObtainBoundSampler(u32 offset) {
    if (!bound_samplers) {
        bound_samplers = std::make_shared<BoundSamplerMap>();
    }
    auto& key_map = *bound_samplers;
    const u32 key = offset;
    const auto iter = key_map.find(key);
    if (iter != key_map.end()) {
        return {iter->second};
    }
    if (!IsEngineSet()) {
        return {};
    }
    const Tegra::Engines::SamplerDescriptor value =
        engine->AccessBoundSampler(shader_stage, offset);
    key_map.emplace(key, value);
    return {value};
}

std::optional<Tegra::Engines::SamplerDescriptor> ConstBufferLocker::ObtainBindlessSampler(
    u32 buffer, u32 offset) {
    if (!bindless_samplers) {
        bindless_samplers = std::make_shared<BindlessSamplerMap>();
    }
    auto& key_map = *bindless_samplers;
    const std::pair<u32, u32> key = {buffer, offset};
    const auto iter = key_map.find(key);
    if (iter != key_map.end()) {
        return {iter->second};
    }
    if (!IsEngineSet()) {
        return {};
    }
    const Tegra::Engines::SamplerDescriptor value =
        engine->AccessBindlessSampler(shader_stage, buffer, offset);
    key_map.emplace(key, value);
    return {value};
}

void ConstBufferLocker::InsertKey(u32 buffer, u32 offset, u32 value) {
    if (!keys) {
        keys = std::make_shared<KeyMap>();
    }
    const std::pair<u32, u32> key = {buffer, offset};
    (*keys)[key] = value;
}

void ConstBufferLocker::InsertBoundSampler(u32 offset, Tegra::Engines::SamplerDescriptor sampler) {
    if (!bound_samplers) {
        bound_samplers = std::make_shared<BoundSamplerMap>();
    }
    (*bound_samplers)[offset] = sampler;
}

void ConstBufferLocker::InsertBindlessSampler(u32 buffer, u32 offset,
                                              Tegra::Engines::SamplerDescriptor sampler) {
    if (!bindless_samplers) {
        bindless_samplers = std::make_shared<BindlessSamplerMap>();
    }
    const std::pair<u32, u32> key = {buffer, offset};
    (*bindless_samplers)[key] = sampler;
}

bool ConstBufferLocker::IsConsistant() const {
    if (!IsEngineSet()) {
        return false;
    }
    if (keys) {
        for (const auto& key_val : *keys) {
            const std::pair<u32, u32> key = key_val.first;
            const u32 value = key_val.second;
            const u32 other_value =
                engine->AccessConstBuffer32(shader_stage, key.first, key.second);
            if (other_value != value) {
                return false;
            }
        }
    }
    if (bound_samplers) {
        for (const auto& sampler_val : *bound_samplers) {
            const u32 key = sampler_val.first;
            const Tegra::Engines::SamplerDescriptor value = sampler_val.second;
            const Tegra::Engines::SamplerDescriptor other_value =
                engine->AccessBoundSampler(shader_stage, key);
            if (other_value.raw != value.raw) {
                return false;
            }
        }
    }
    if (bindless_samplers) {
        for (const auto& sampler_val : *bindless_samplers) {
            const std::pair<u32, u32> key = sampler_val.first;
            const Tegra::Engines::SamplerDescriptor value = sampler_val.second;
            const Tegra::Engines::SamplerDescriptor other_value =
                engine->AccessBindlessSampler(shader_stage, key.first, key.second);
            if (other_value.raw != value.raw) {
                return false;
            }
        }
    }
    return true;
}

} // namespace VideoCommon::Shader
