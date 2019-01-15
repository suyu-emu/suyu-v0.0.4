// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <set>
#include <string>
#include <tuple>
#include <vector>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "video_core/engines/maxwell_3d.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"

namespace OpenGL {

using ProgramCode = std::vector<u64>;
using Maxwell = Tegra::Engines::Maxwell3D::Regs;

struct BaseBindings {
private:
    auto Tie() const {
        return std::tie(cbuf, gmem, sampler);
    }

public:
    u32 cbuf{};
    u32 gmem{};
    u32 sampler{};

    bool operator<(const BaseBindings& rhs) const {
        return Tie() < rhs.Tie();
    }

    bool operator==(const BaseBindings& rhs) const {
        return Tie() == rhs.Tie();
    }

    bool operator!=(const BaseBindings& rhs) const {
        return !this->operator==(rhs);
    }
};

class ShaderDiskCacheRaw {
public:
    explicit ShaderDiskCacheRaw(FileUtil::IOFile& file);

    explicit ShaderDiskCacheRaw(u64 unique_identifier, Maxwell::ShaderProgram program_type,
                                u32 program_code_size, u32 program_code_size_b,
                                ProgramCode program_code, ProgramCode program_code_b)
        : unique_identifier{unique_identifier}, program_type{program_type},
          program_code_size{program_code_size}, program_code_size_b{program_code_size_b},
          program_code{std::move(program_code)}, program_code_b{std::move(program_code_b)} {}

    void Save(FileUtil::IOFile& file) const;

    u64 GetUniqueIdentifier() const {
        return unique_identifier;
    }

    bool HasProgramA() const {
        return program_type == Maxwell::ShaderProgram::VertexA;
    }

    Maxwell::ShaderProgram GetProgramType() const {
        return program_type;
    }

    Maxwell::ShaderStage GetProgramStage() const {
        switch (program_type) {
        case Maxwell::ShaderProgram::VertexA:
        case Maxwell::ShaderProgram::VertexB:
            return Maxwell::ShaderStage::Vertex;
        case Maxwell::ShaderProgram::TesselationControl:
            return Maxwell::ShaderStage::TesselationControl;
        case Maxwell::ShaderProgram::TesselationEval:
            return Maxwell::ShaderStage::TesselationEval;
        case Maxwell::ShaderProgram::Geometry:
            return Maxwell::ShaderStage::Geometry;
        case Maxwell::ShaderProgram::Fragment:
            return Maxwell::ShaderStage::Fragment;
        }
        UNREACHABLE();
    }

    const ProgramCode& GetProgramCode() const {
        return program_code;
    }

    const ProgramCode& GetProgramCodeB() const {
        return program_code_b;
    }

private:
    u64 unique_identifier{};
    Maxwell::ShaderProgram program_type{};
    u32 program_code_size{};
    u32 program_code_size_b{};

    ProgramCode program_code;
    ProgramCode program_code_b;
};

struct ShaderDiskCacheUsage {
private:
    auto Tie() const {
        return std::tie(unique_identifier, bindings, primitive);
    }

public:
    u64 unique_identifier{};
    BaseBindings bindings;
    GLenum primitive{};

    bool operator<(const ShaderDiskCacheUsage& rhs) const {
        return Tie() < rhs.Tie();
    }

    bool operator==(const ShaderDiskCacheUsage& rhs) const {
        return Tie() == rhs.Tie();
    }

    bool operator!=(const ShaderDiskCacheUsage& rhs) const {
        return !this->operator==(rhs);
    }
};

struct ShaderDiskCacheDecompiled {
    std::string code;
    GLShader::ShaderEntries entries;
};

struct ShaderDiskCacheDump {
    GLenum binary_format;
    std::vector<u8> binary;
};

class ShaderDiskCacheOpenGL {
public:
    /// Loads transferable cache. If file has a old version, it deletes it. Returns true on success.
    bool LoadTransferable(std::vector<ShaderDiskCacheRaw>& raws,
                          std::vector<ShaderDiskCacheUsage>& usages);

    /// Loads current game's precompiled cache. Invalidates if emulator's version has changed.
    bool LoadPrecompiled(std::map<u64, ShaderDiskCacheDecompiled>& decompiled,
                         std::map<ShaderDiskCacheUsage, ShaderDiskCacheDump>& dumps);

    /// Removes the transferable (and precompiled) cache file.
    void InvalidateTransferable() const;

    /// Removes the precompiled cache file.
    void InvalidatePrecompiled() const;

    /// Saves a raw dump to the transferable file. Checks for collisions.
    void SaveRaw(const ShaderDiskCacheRaw& entry);

    /// Saves shader usage to the transferable file. Does not check for collisions.
    void SaveUsage(const ShaderDiskCacheUsage& usage);

    /// Saves a decompiled entry to the precompiled file. Does not check for collisions.
    void SaveDecompiled(u64 unique_identifier, const std::string& code,
                        const GLShader::ShaderEntries& entries);

    /// Saves a dump entry to the precompiled file. Does not check for collisions.
    void SaveDump(const ShaderDiskCacheUsage& usage, GLuint program);

private:
    /// Opens current game's transferable file and write it's header if it doesn't exist
    FileUtil::IOFile AppendTransferableFile() const;

    /// Opens current game's precompiled file and write it's header if it doesn't exist
    FileUtil::IOFile AppendPrecompiledFile() const;

    /// Create shader disk cache directories. Returns true on success.
    bool EnsureDirectories() const;

    /// Gets current game's transferable file path
    std::string GetTransferablePath() const;

    /// Gets current game's precompiled file path
    std::string GetPrecompiledPath() const;

    /// Get user's transferable directory path
    std::string GetTransferableDir() const;

    /// Get user's precompiled directory path
    std::string GetPrecompiledDir() const;

    /// Get user's shader directory path
    std::string GetBaseDir() const;

    // Stored transferable shaders
    std::map<u64, std::set<ShaderDiskCacheUsage>> transferable;
};

} // namespace OpenGL