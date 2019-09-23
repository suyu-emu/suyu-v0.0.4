// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "common/common_types.h"
#include "common/hash.h"
#include "video_core/engines/const_buffer_engine_interface.h"

namespace VideoCommon::Shader {

class ConstBufferLocker {
public:
    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage);

    explicit ConstBufferLocker(Tegra::Engines::ShaderType shader_stage,
                               Tegra::Engines::ConstBufferEngineInterface* engine);

    // Checks if an engine is setup, it may be possible that during disk shader
    // cache run, the engines have not been created yet.
    bool IsEngineSet() const;

    // Use this to set/change the engine used for this shader.
    void SetEngine(Tegra::Engines::ConstBufferEngineInterface* engine);

    // Retrieves a key from the locker, if it's registered, it will give the
    // registered value, if not it will obtain it from maxwell3d and register it.
    std::optional<u32> ObtainKey(u32 buffer, u32 offset);

    // Manually inserts a key.
    void InsertKey(u32 buffer, u32 offset, u32 value);

    // Retrieves the number of keys registered.
    u32 NumKeys() const;

    // Gives an accessor to the key's database.
    const std::unordered_map<std::pair<u32, u32>, u32, Common::PairHash>& AccessKeys() const;

    // Checks keys against maxwell3d's current const buffers. Returns true if they
    // are the same value, false otherwise;
    bool AreKeysConsistant() const;

private:
    Tegra::Engines::ConstBufferEngineInterface* engine;
    Tegra::Engines::ShaderType shader_stage;
    std::unordered_map<std::pair<u32, u32>, u32, Common::PairHash> keys{};
};
} // namespace VideoCommon::Shader
