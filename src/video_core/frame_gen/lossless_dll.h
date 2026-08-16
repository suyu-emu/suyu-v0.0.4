// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2025 lsfg-vk
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <filesystem>
#include <map>
#include <vector>

#include "common/common_types.h"

namespace VideoCore::FrameGen {

enum class LosslessStatus : u32 {
    Ok,
    NotInstalled,
    UnreadableFile,
    NotPortableExecutable,
    MissingShaders,
    TranslationFailed,
    CacheUnusable,
};

using ShaderResources = std::map<u32, std::vector<u8>>;
using ShaderModules = std::map<u32, std::vector<u32>>;

enum class ShaderVariant : u32 {
    NativeFp32 = 1,
    NativeFp16 = 2,
};

namespace PerformanceShader {
constexpr u32 MIPMAPS = 255;
constexpr u32 GENERATE = 256;
constexpr std::array<u32, 4> ALPHA{290, 291, 292, 293};
constexpr std::array<u32, 5> BETA{298, 299, 300, 301, 302};
constexpr std::array<u32, 5> GAMMA{280, 282, 283, 284, 285};
constexpr std::array<u32, 10> DELTA{280, 286, 287, 288, 289, 281, 294, 295, 296, 297};

constexpr u32 NATIVE_FP16_OFFSET = 49;
constexpr u32 NATIVE_FP32_OFFSET = 98;
} // namespace PerformanceShader

[[nodiscard]] std::filesystem::path GetLosslessDllPath();

[[nodiscard]] std::filesystem::path GetShaderCachePath();

[[nodiscard]] LosslessStatus ReadShaderResources(const std::filesystem::path& path,
                                                 ShaderResources& out_resources);

[[nodiscard]] LosslessStatus ValidateLosslessDll(const std::filesystem::path& path);

[[nodiscard]] LosslessStatus GetInstalledLosslessStatus();

[[nodiscard]] LosslessStatus BuildShaderCache();

[[nodiscard]] LosslessStatus LoadShaderModules(ShaderModules& out_modules,
                                              bool allow_fp16 = false,
                                              bool prefer_fp16 = false);

bool RemoveInstalledLosslessDll();

} // namespace VideoCore::FrameGen
