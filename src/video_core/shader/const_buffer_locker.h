// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "common/common_types.h"
#include "common/hash.h"
#include "video_core/engines/const_buffer_engine_interface.h"

namespace VideoCommon::Shader {

using KeyMap = std::unordered_map<std::pair<u32, u32>, u32, Common::PairHash>;
using BoundSamplerMap = std::unordered_map<u32, Tegra::Engines::SamplerDescriptor>;
using BindlessSamplerMap =
    std::unordered_map<std::pair<u32, u32>, Tegra::Engines::SamplerDescriptor, Common::PairHash>;

class ConstBufferLocker {
public:
    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage);

    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage,
                               Tegra::Engines::ConstBufferEngineInterface& engine);

    // Checks if an engine is setup, it may be possible that during disk shader
    // cache run, the engines have not been created yet.
    bool IsEngineSet() const;

    // Use this to set/change the engine used for this shader.
    void SetEngine(Tegra::Engines::ConstBufferEngineInterface& engine);

    // Retrieves a key from the locker, if it's registered, it will give the
    // registered value, if not it will obtain it from maxwell3d and register it.
    std::optional<u32> ObtainKey(u32 buffer, u32 offset);

    std::optional<Tegra::Engines::SamplerDescriptor> ObtainBoundSampler(u32 offset);

    std::optional<Tegra::Engines::SamplerDescriptor> ObtainBindlessSampler(u32 buffer, u32 offset);

    // Manually inserts a key.
    void InsertKey(u32 buffer, u32 offset, u32 value);

    void InsertBoundSampler(u32 offset, Tegra::Engines::SamplerDescriptor sampler);

    void InsertBindlessSampler(u32 buffer, u32 offset, Tegra::Engines::SamplerDescriptor sampler);

    // Retrieves the number of keys registered.
    std::size_t NumKeys() const {
        if (!keys) {
            return 0;
        }
        return keys->size();
    }

    std::size_t NumBoundSamplers() const {
        if (!bound_samplers) {
            return 0;
        }
        return bound_samplers->size();
    }

    std::size_t NumBindlessSamplers() const {
        if (!bindless_samplers) {
            return 0;
        }
        return bindless_samplers->size();
    }

    // Gives an accessor to the key's database.
    // Pre: NumKeys > 0
    const KeyMap& AccessKeys() const {
        return *keys;
    }

    // Gives an accessor to the sampler's database.
    // Pre: NumBindlessSamplers > 0
    const BoundSamplerMap& AccessBoundSamplers() const {
        return *bound_samplers;
    }

    // Gives an accessor to the sampler's database.
    // Pre: NumBindlessSamplers > 0
    const BindlessSamplerMap& AccessBindlessSamplers() const {
        return *bindless_samplers;
    }

    // Checks keys & samplers against engine's current const buffers. Returns true if they
    // are the same value, false otherwise;
    bool IsConsistent() const;

private:
    Tegra::Engines::ConstBufferEngineInterface* engine;
    Tegra::Engines::ShaderType shader_stage;
    // All containers are lazy initialized as most shaders don't use them.
    std::shared_ptr<KeyMap> keys{};
    std::shared_ptr<BoundSamplerMap> bound_samplers{};
    std::shared_ptr<BindlessSamplerMap> bindless_samplers{};
};
} // namespace VideoCommon::Shader
