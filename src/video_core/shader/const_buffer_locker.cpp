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
    const std::pair<u32, u32> key = {buffer, offset};
    const auto iter = keys.find(key);
    if (iter != keys.end()) {
        return {iter->second};
    }
    if (!IsEngineSet()) {
        return {};
    }
    const u32 value = engine->AccessConstBuffer32(shader_stage, buffer, offset);
    keys.emplace(key, value);
    return {value};
}

void ConstBufferLocker::InsertKey(u32 buffer, u32 offset, u32 value) {
    const std::pair<u32, u32> key = {buffer, offset};
    keys[key] = value;
}

u32 ConstBufferLocker::NumKeys() const {
    return keys.size();
}

const std::unordered_map<std::pair<u32, u32>, u32, Common::PairHash>&
ConstBufferLocker::AccessKeys() const {
    return keys;
}

bool ConstBufferLocker::AreKeysConsistant() const {
    if (!IsEngineSet()) {
        return false;
    }
    for (const auto& key_val : keys) {
        const std::pair<u32, u32> key = key_val.first;
        const u32 value = key_val.second;
        const u32 other_value = engine->AccessConstBuffer32(shader_stage, key.first, key.second);
        if (other_value != value) {
            return false;
        }
    }
    return true;
}

} // namespace VideoCommon::Shader
