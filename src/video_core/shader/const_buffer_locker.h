// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <unordered_map>
#include "common/common_types.h"
#include "common/hash.h"
#include "video_core/engines/const_buffer_engine_interface.h"
#include "video_core/engines/shader_type.h"
#include "video_core/guest_driver.h"

namespace VideoCommon::Shader {

using KeyMap = std::unordered_map<std::pair<u32, u32>, u32, Common::PairHash>;
using BoundSamplerMap = std::unordered_map<u32, Tegra::Engines::SamplerDescriptor>;
using BindlessSamplerMap =
    std::unordered_map<std::pair<u32, u32>, Tegra::Engines::SamplerDescriptor, Common::PairHash>;

/**
 * The ConstBufferLocker is a class use to interface the 3D and compute engines with the shader
 * compiler. with it, the shader can obtain required data from GPU state and store it for disk
 * shader compilation.
 */
class ConstBufferLocker {
public:
    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage);

    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage,
                               Tegra::Engines::ConstBufferEngineInterface& engine);

    ~ConstBufferLocker();

    /// Retrieves a key from the locker, if it's registered, it will give the registered value, if
    /// not it will obtain it from maxwell3d and register it.
    std::optional<u32> ObtainKey(u32 buffer, u32 offset);

    std::optional<Tegra::Engines::SamplerDescriptor> ObtainBoundSampler(u32 offset);

    std::optional<Tegra::Engines::SamplerDescriptor> ObtainBindlessSampler(u32 buffer, u32 offset);

    std::optional<u32> ObtainBoundBuffer();

    /// Inserts a key.
    void InsertKey(u32 buffer, u32 offset, u32 value);

    /// Inserts a bound sampler key.
    void InsertBoundSampler(u32 offset, Tegra::Engines::SamplerDescriptor sampler);

    /// Inserts a bindless sampler key.
    void InsertBindlessSampler(u32 buffer, u32 offset, Tegra::Engines::SamplerDescriptor sampler);

    /// Set the bound buffer for this locker.
    void SetBoundBuffer(u32 buffer);

    /// Checks keys and samplers against engine's current const buffers. Returns true if they are
    /// the same value, false otherwise;
    bool IsConsistent() const;

    /// Returns true if the keys are equal to the other ones in the locker.
    bool HasEqualKeys(const ConstBufferLocker& rhs) const;

    /// Gives an getter to the const buffer keys in the database.
    const KeyMap& GetKeys() const {
        return keys;
    }

    /// Gets samplers database.
    const BoundSamplerMap& GetBoundSamplers() const {
        return bound_samplers;
    }

    /// Gets bindless samplers database.
    const BindlessSamplerMap& GetBindlessSamplers() const {
        return bindless_samplers;
    }

    /// Gets bound buffer used on this shader
    u32 GetBoundBuffer() const {
        return bound_buffer;
    }

    /// Obtains access to the guest driver's profile.
    VideoCore::GuestDriverProfile* AccessGuestDriverProfile() const {
        if (engine) {
            return &engine->AccessGuestDriverProfile();
        }
        return nullptr;
    }

private:
    const Tegra::Engines::ShaderType stage;
    Tegra::Engines::ConstBufferEngineInterface* engine = nullptr;
    KeyMap keys;
    BoundSamplerMap bound_samplers;
    BindlessSamplerMap bindless_samplers;
    bool bound_buffer_saved{};
    u32 bound_buffer{};
};

} // namespace VideoCommon::Shader
