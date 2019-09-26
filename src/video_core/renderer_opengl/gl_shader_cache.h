// Copyright 2018 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <bitset>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <glad/glad.h>

#include "common/common_types.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"
#include "video_core/shader/const_buffer_locker.h"
#include "video_core/shader/shader_ir.h"

namespace Core {
class System;
}

namespace Core::Frontend {
class EmuWindow;
}

namespace OpenGL {

class CachedShader;
class Device;
class RasterizerOpenGL;
struct UnspecializedShader;

using Shader = std::shared_ptr<CachedShader>;
using CachedProgram = std::shared_ptr<OGLProgram>;
using Maxwell = Tegra::Engines::Maxwell3D::Regs;
using PrecompiledPrograms = std::unordered_map<ShaderDiskCacheUsage, CachedProgram>;
using PrecompiledVariants = std::vector<PrecompiledPrograms::iterator>;

struct UnspecializedShader {
    GLShader::ShaderEntries entries;
    ProgramType program_type;
    ProgramCode code;
    ProgramCode code_b;
};

struct ShaderParameters {
    Core::System& system;
    ShaderDiskCacheOpenGL& disk_cache;
    const PrecompiledVariants* precompiled_variants;
    const Device& device;
    VAddr cpu_addr;
    u8* host_ptr;
    u64 unique_identifier;
};

class CachedShader final : public RasterizerCacheObject {
public:
    static Shader CreateStageFromMemory(const ShaderParameters& params,
                                        Maxwell::ShaderProgram program_type,
                                        ProgramCode program_code, ProgramCode program_code_b);
    static Shader CreateKernelFromMemory(const ShaderParameters& params, ProgramCode code);

    static Shader CreateFromCache(const ShaderParameters& params,
                                  const UnspecializedShader& unspecialized);

    VAddr GetCpuAddr() const override {
        return cpu_addr;
    }

    std::size_t GetSizeInBytes() const override {
        return program_code.size() * sizeof(u64);
    }

    /// Gets the shader entries for the shader
    const GLShader::ShaderEntries& GetShaderEntries() const {
        return entries;
    }

    /// Gets the GL program handle for the shader
    std::tuple<GLuint, BaseBindings> GetProgramHandle(const ProgramVariant& variant);

private:
    struct LockerVariant {
        std::unique_ptr<VideoCommon::Shader::ConstBufferLocker> locker;
        std::unordered_map<ProgramVariant, CachedProgram> programs;
    };

    explicit CachedShader(const ShaderParameters& params, ProgramType program_type,
                          GLShader::ShaderEntries entries, ProgramCode program_code,
                          ProgramCode program_code_b);

    void UpdateVariant();

    ShaderDiskCacheUsage GetUsage(const ProgramVariant& variant,
                                  const VideoCommon::Shader::ConstBufferLocker& locker) const;

    Core::System& system;
    ShaderDiskCacheOpenGL& disk_cache;
    const Device& device;

    VAddr cpu_addr{};

    u64 unique_identifier{};
    ProgramType program_type{};

    GLShader::ShaderEntries entries;

    ProgramCode program_code;
    ProgramCode program_code_b;

    LockerVariant* curr_variant = nullptr;
    std::vector<std::unique_ptr<LockerVariant>> locker_variants;
};

class ShaderCacheOpenGL final : public RasterizerCache<Shader> {
public:
    explicit ShaderCacheOpenGL(RasterizerOpenGL& rasterizer, Core::System& system,
                               Core::Frontend::EmuWindow& emu_window, const Device& device);

    /// Loads disk cache for the current game
    void LoadDiskCache(const std::atomic_bool& stop_loading,
                       const VideoCore::DiskResourceLoadCallback& callback);

    /// Gets the current specified shader stage program
    Shader GetStageProgram(Maxwell::ShaderProgram program);

    /// Gets a compute kernel in the passed address
    Shader GetComputeKernel(GPUVAddr code_addr);

protected:
    // We do not have to flush this cache as things in it are never modified by us.
    void FlushObjectInner(const Shader& object) override {}

private:
    bool GenerateUnspecializedShaders(const std::atomic_bool& stop_loading,
                                      const VideoCore::DiskResourceLoadCallback& callback,
                                      const std::vector<ShaderDiskCacheRaw>& raws);

    CachedProgram GeneratePrecompiledProgram(const ShaderDiskCacheDump& dump,
                                             const std::unordered_set<GLenum>& supported_formats);

    const PrecompiledVariants* GetPrecompiledVariants(u64 unique_identifier) const;

    Core::System& system;
    Core::Frontend::EmuWindow& emu_window;
    const Device& device;

    ShaderDiskCacheOpenGL disk_cache;

    PrecompiledPrograms precompiled_programs;
    std::unordered_map<u64, PrecompiledVariants> precompiled_variants;

    std::unordered_map<u64, UnspecializedShader> unspecialized_shaders;

    std::array<Shader, Maxwell::MaxShaderProgram> last_shaders;
};

} // namespace OpenGL
