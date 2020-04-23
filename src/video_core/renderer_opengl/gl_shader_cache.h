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
#include "video_core/engines/shader_type.h"
#include "video_core/rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/renderer_opengl/gl_shader_decompiler.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"
#include "video_core/shader/registry.h"
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
using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct PrecompiledShader {
    std::shared_ptr<OGLProgram> program;
    std::shared_ptr<VideoCommon::Shader::Registry> registry;
    ShaderEntries entries;
};

struct ShaderParameters {
    Core::System& system;
    ShaderDiskCacheOpenGL& disk_cache;
    const Device& device;
    VAddr cpu_addr;
    u8* host_ptr;
    u64 unique_identifier;
};

class CachedShader final : public RasterizerCacheObject {
public:
    ~CachedShader();

    /// Gets the GL program handle for the shader
    GLuint GetHandle() const;

    /// Returns the size in bytes of the shader
    std::size_t GetSizeInBytes() const override {
        return size_in_bytes;
    }

    /// Gets the shader entries for the shader
    const ShaderEntries& GetEntries() const {
        return entries;
    }

    static Shader CreateStageFromMemory(const ShaderParameters& params,
                                        Maxwell::ShaderProgram program_type,
                                        ProgramCode program_code, ProgramCode program_code_b);
    static Shader CreateKernelFromMemory(const ShaderParameters& params, ProgramCode code);

    static Shader CreateFromCache(const ShaderParameters& params,
                                  const PrecompiledShader& precompiled_shader,
                                  std::size_t size_in_bytes);

private:
    explicit CachedShader(VAddr cpu_addr, std::size_t size_in_bytes,
                          std::shared_ptr<VideoCommon::Shader::Registry> registry,
                          ShaderEntries entries, std::shared_ptr<OGLProgram> program);

    std::shared_ptr<VideoCommon::Shader::Registry> registry;
    ShaderEntries entries;
    std::size_t size_in_bytes = 0;
    std::shared_ptr<OGLProgram> program;
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
    std::shared_ptr<OGLProgram> GeneratePrecompiledProgram(
        const ShaderDiskCacheEntry& entry, const ShaderDiskCachePrecompiled& precompiled_entry,
        const std::unordered_set<GLenum>& supported_formats);

    Core::System& system;
    Core::Frontend::EmuWindow& emu_window;
    const Device& device;
    ShaderDiskCacheOpenGL disk_cache;
    std::unordered_map<u64, PrecompiledShader> runtime_cache;

    Shader null_shader{};
    Shader null_kernel{};

    std::array<Shader, Maxwell::MaxShaderProgram> last_shaders;
};

} // namespace OpenGL
