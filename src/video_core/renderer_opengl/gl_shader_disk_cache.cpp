// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstring>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"

#include "core/core.h"
#include "core/hle/kernel/process.h"

#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

namespace OpenGL {

enum class EntryKind : u32 {
    Raw,
    Usage,
};

constexpr u32 NativeVersion = 1;
constexpr u32 ShaderHashSize = 64;

// Making sure sizes doesn't change by accident
static_assert(sizeof(BaseBindings) == 12);
static_assert(sizeof(ShaderDiskCacheUsage) == 24);

namespace {
std::string GetTitleID() {
    return fmt::format("{:016X}", Core::CurrentProcess()->GetTitleID());
}

std::string GetShaderHash() {
    std::array<char, ShaderHashSize> hash{};
    std::strncpy(hash.data(), Common::g_shader_cache_version, ShaderHashSize);
    return std::string(hash.data());
}
} // namespace

ShaderDiskCacheRaw::ShaderDiskCacheRaw(FileUtil::IOFile& file) {
    file.ReadBytes(&unique_identifier, sizeof(u64));
    file.ReadBytes(&program_type, sizeof(u32));

    u32 program_code_size{}, program_code_size_b{};
    file.ReadBytes(&program_code_size, sizeof(u32));
    file.ReadBytes(&program_code_size_b, sizeof(u32));

    program_code.resize(program_code_size);
    program_code_b.resize(program_code_size_b);

    file.ReadArray(program_code.data(), program_code_size);
    if (HasProgramA()) {
        file.ReadArray(program_code_b.data(), program_code_size_b);
    }
}

void ShaderDiskCacheRaw::Save(FileUtil::IOFile& file) const {
    file.WriteObject(unique_identifier);
    file.WriteObject(static_cast<u32>(program_type));
    file.WriteObject(program_code_size);
    file.WriteObject(program_code_size_b);

    file.WriteArray(program_code.data(), program_code_size);
    if (HasProgramA()) {
        file.WriteArray(program_code_b.data(), program_code_size_b);
    }
}

bool ShaderDiskCacheOpenGL::LoadTransferable(std::vector<ShaderDiskCacheRaw>& raws,
                                             std::vector<ShaderDiskCacheUsage>& usages) {
    FileUtil::IOFile file(GetTransferablePath(), "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No transferable shader cache found for game with title id={}",
                 GetTitleID());
        return false;
    }
    const u64 file_size = file.GetSize();

    u32 version{};
    file.ReadBytes(&version, sizeof(version));

    if (version < NativeVersion) {
        LOG_INFO(Render_OpenGL, "Transferable shader cache is old - removing");
        file.Close();
        FileUtil::Delete(GetTransferablePath());
        return false;
    }
    if (version > NativeVersion) {
        LOG_WARNING(Render_OpenGL, "Transferable shader cache was generated with a newer version "
                                   "of the emulator - skipping");
        return false;
    }

    // Version is valid, load the shaders
    while (file.Tell() < file_size) {
        EntryKind kind{};
        file.ReadBytes(&kind, sizeof(u32));

        switch (kind) {
        case EntryKind::Raw: {
            ShaderDiskCacheRaw entry{file};
            transferable.insert({entry.GetUniqueIdentifier(), {}});
            raws.push_back(std::move(entry));
            break;
        }
        case EntryKind::Usage: {
            ShaderDiskCacheUsage usage{};
            file.ReadBytes(&usage, sizeof(usage));
            usages.push_back(std::move(usage));
            break;
        }
        default:
            LOG_ERROR(Render_OpenGL, "Unknown transferable shader cache entry kind={} - aborting",
                      static_cast<u32>(kind));
            return false;
        }
    }
    return true;
}

std::vector<ShaderDiskCachePrecompiledEntry> ShaderDiskCacheOpenGL::LoadPrecompiled() {
    FileUtil::IOFile file(GetPrecompiledPath(), "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No precompiled shader cache found for game with title id={}",
                 GetTitleID());
        return {};
    }
    const u64 file_size = file.GetSize();

    char precompiled_hash[ShaderHashSize];
    file.ReadBytes(&precompiled_hash, ShaderHashSize);
    if (std::string(precompiled_hash) != GetShaderHash()) {
        LOG_INFO(Render_OpenGL, "Precompiled cache is from another version of yuzu - removing");
        file.Close();
        InvalidatePrecompiled();
        return {};
    }

    std::vector<ShaderDiskCachePrecompiledEntry> precompiled;
    while (file.Tell() < file_size) {
        ShaderDiskCachePrecompiledEntry entry;
        file.ReadBytes(&entry.usage, sizeof(entry.usage));

        file.ReadBytes(&entry.binary_format, sizeof(u32));

        u32 binary_length{};
        file.ReadBytes(&binary_length, sizeof(u32));
        entry.binary.resize(binary_length);
        file.ReadBytes(entry.binary.data(), entry.binary.size());

        precompiled.push_back(entry);
    }
    return precompiled;
}

void ShaderDiskCacheOpenGL::InvalidateTransferable() const {
    FileUtil::Delete(GetTransferablePath());
    InvalidatePrecompiled();
}

void ShaderDiskCacheOpenGL::InvalidatePrecompiled() const {
    FileUtil::Delete(GetPrecompiledPath());
}

void ShaderDiskCacheOpenGL::SaveRaw(const ShaderDiskCacheRaw& entry) {
    const u64 id = entry.GetUniqueIdentifier();
    if (transferable.find(id) != transferable.end()) {
        // The shader already exists
        return;
    }

    FileUtil::IOFile file = AppendTransferableFile();
    if (!file.IsOpen()) {
        return;
    }
    file.WriteObject(EntryKind::Raw);
    entry.Save(file);

    transferable.insert({id, {}});
}

void ShaderDiskCacheOpenGL::SaveUsage(const ShaderDiskCacheUsage& usage) {
    const auto it = transferable.find(usage.unique_identifier);
    if (it == transferable.end()) {
        LOG_CRITICAL(Render_OpenGL, "Saving shader usage without storing raw previously");
        UNREACHABLE();
    }
    auto& usages{it->second};
    ASSERT(usages.find(usage) == usages.end());
    usages.insert(usage);

    FileUtil::IOFile file = AppendTransferableFile();
    if (!file.IsOpen()) {
        return;
    }
    file.WriteObject(EntryKind::Usage);
    file.WriteObject(usage);
}

void ShaderDiskCacheOpenGL::SavePrecompiled(const ShaderDiskCacheUsage& usage, GLuint program) {
    FileUtil::IOFile file = AppendPrecompiledFile();
    if (!file.IsOpen()) {
        return;
    }

    file.WriteObject(usage);

    GLint binary_length{};
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binary_length);

    GLenum binary_format{};
    std::vector<u8> binary(binary_length);
    glGetProgramBinary(program, binary_length, nullptr, &binary_format, binary.data());

    file.WriteObject(static_cast<u32>(binary_format));
    file.WriteObject(static_cast<u32>(binary_length));
    file.WriteArray(binary.data(), binary.size());
}

FileUtil::IOFile ShaderDiskCacheOpenGL::AppendTransferableFile() const {
    if (!EnsureDirectories()) {
        return {};
    }

    const auto transferable_path{GetTransferablePath()};
    const bool existed = FileUtil::Exists(transferable_path);

    FileUtil::IOFile file(transferable_path, "ab");
    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open transferable cache in path={}", transferable_path);
        return {};
    }
    if (!existed || file.GetSize() == 0) {
        // If the file didn't exist, write its version
        file.WriteObject(NativeVersion);
    }
    return file;
}

FileUtil::IOFile ShaderDiskCacheOpenGL::AppendPrecompiledFile() const {
    if (!EnsureDirectories()) {
        return {};
    }

    const auto precompiled_path{GetPrecompiledPath()};
    const bool existed = FileUtil::Exists(precompiled_path);

    FileUtil::IOFile file(precompiled_path, "ab");
    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open precompiled cache in path={}", precompiled_path);
        return {};
    }

    if (!existed || file.GetSize() == 0) {
        std::array<char, ShaderHashSize> hash{};
        std::strcpy(hash.data(), GetShaderHash().c_str());
        file.WriteArray(hash.data(), hash.size());
    }
    return file;
}

bool ShaderDiskCacheOpenGL::EnsureDirectories() const {
    const auto CreateDir = [](const std::string& dir) {
        if (!FileUtil::CreateDir(dir)) {
            LOG_ERROR(Render_OpenGL, "Failed to create directory={}", dir);
            return false;
        }
        return true;
    };

    return CreateDir(FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir)) &&
           CreateDir(GetBaseDir()) && CreateDir(GetTransferableDir()) &&
           CreateDir(GetPrecompiledDir());
}

std::string ShaderDiskCacheOpenGL::GetTransferablePath() const {
    return FileUtil::SanitizePath(GetTransferableDir() + DIR_SEP_CHR + GetTitleID() + ".bin");
}

std::string ShaderDiskCacheOpenGL::GetPrecompiledPath() const {
    return FileUtil::SanitizePath(GetPrecompiledDir() + DIR_SEP_CHR + GetTitleID() + ".bin");
}

std::string ShaderDiskCacheOpenGL::GetTransferableDir() const {
    return GetBaseDir() + DIR_SEP "transferable";
}

std::string ShaderDiskCacheOpenGL::GetPrecompiledDir() const {
    return GetBaseDir() + DIR_SEP "precompiled";
}

std::string ShaderDiskCacheOpenGL::GetBaseDir() const {
    return FileUtil::GetUserPath(FileUtil::UserPath::ShaderDir) + DIR_SEP "opengl";
}

} // namespace OpenGL