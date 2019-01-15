// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <fmt/format.h>
#include <lz4.h>

#include "common/assert.h"
#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"

#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/settings.h"

#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

namespace OpenGL {

enum class TransferableEntryKind : u32 {
    Raw,
    Usage,
};

enum class PrecompiledEntryKind : u32 {
    Decompiled,
    Dump,
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

template <typename T>
std::vector<u8> CompressData(const T* source, std::size_t source_size) {
    const auto source_size_int = static_cast<int>(source_size);
    const int max_compressed_size = LZ4_compressBound(source_size_int);
    std::vector<u8> compressed(max_compressed_size);
    const int compressed_size = LZ4_compress_default(reinterpret_cast<const char*>(source),
                                                     reinterpret_cast<char*>(compressed.data()),
                                                     source_size_int, max_compressed_size);
    if (compressed_size < 0) {
        // Compression failed
        return {};
    }
    compressed.resize(compressed_size);
    return compressed;
}

std::vector<u8> DecompressData(const std::vector<u8>& compressed, std::size_t uncompressed_size) {
    std::vector<u8> uncompressed(uncompressed_size);
    const int size_check = LZ4_decompress_safe(reinterpret_cast<const char*>(compressed.data()),
                                               reinterpret_cast<char*>(uncompressed.data()),
                                               static_cast<int>(compressed.size()),
                                               static_cast<int>(uncompressed.size()));
    if (static_cast<int>(uncompressed_size) != size_check) {
        // Decompression failed
        return {};
    }
    return uncompressed;
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
    if (!Settings::values.use_disk_shader_cache) {
        return false;
    }

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
        TransferableEntryKind kind{};
        file.ReadBytes(&kind, sizeof(u32));

        switch (kind) {
        case TransferableEntryKind::Raw: {
            ShaderDiskCacheRaw entry{file};
            transferable.insert({entry.GetUniqueIdentifier(), {}});
            raws.push_back(std::move(entry));
            break;
        }
        case TransferableEntryKind::Usage: {
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

bool ShaderDiskCacheOpenGL::LoadPrecompiled(
    std::map<u64, ShaderDiskCacheDecompiled>& decompiled,
    std::map<ShaderDiskCacheUsage, ShaderDiskCacheDump>& dumps) {

    if (!Settings::values.use_disk_shader_cache) {
        return false;
    }

    FileUtil::IOFile file(GetPrecompiledPath(), "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No precompiled shader cache found for game with title id={}",
                 GetTitleID());
        return false;
    }
    const u64 file_size = file.GetSize();

    char precompiled_hash[ShaderHashSize];
    file.ReadBytes(&precompiled_hash, ShaderHashSize);
    if (std::string(precompiled_hash) != GetShaderHash()) {
        LOG_INFO(Render_OpenGL, "Precompiled cache is from another version of yuzu - removing");
        file.Close();
        InvalidatePrecompiled();
        return false;
    }

    while (file.Tell() < file_size) {
        PrecompiledEntryKind kind{};
        file.ReadBytes(&kind, sizeof(u32));

        switch (kind) {
        case PrecompiledEntryKind::Decompiled: {
            ShaderDiskCacheDecompiled entry;

            u64 unique_identifier{};
            file.ReadBytes(&unique_identifier, sizeof(u64));

            u32 code_size{};
            u32 compressed_code_size{};
            file.ReadBytes(&code_size, sizeof(u32));
            file.ReadBytes(&compressed_code_size, sizeof(u32));

            std::vector<u8> compressed_code(compressed_code_size);
            file.ReadArray(compressed_code.data(), compressed_code.size());

            const std::vector<u8> code = DecompressData(compressed_code, code_size);
            if (code.empty()) {
                LOG_ERROR(Render_OpenGL,
                          "Failed to decompress GLSL code in precompiled shader={:016x} - removing",
                          unique_identifier);
                InvalidatePrecompiled();
                dumps.clear();
                decompiled.clear();
                return false;
            }
            entry.code = std::string(reinterpret_cast<const char*>(code.data()), code_size);

            u32 const_buffers_count{};
            file.ReadBytes(&const_buffers_count, sizeof(u32));
            for (u32 i = 0; i < const_buffers_count; ++i) {
                u32 max_offset{}, index{};
                u8 is_indirect{};
                file.ReadBytes(&max_offset, sizeof(u32));
                file.ReadBytes(&index, sizeof(u32));
                file.ReadBytes(&is_indirect, sizeof(u8));

                entry.entries.const_buffers.emplace_back(max_offset, is_indirect != 0, index);
            }

            u32 samplers_count{};
            file.ReadBytes(&samplers_count, sizeof(u32));
            for (u32 i = 0; i < samplers_count; ++i) {
                u64 offset{}, index{};
                u32 type{};
                u8 is_array{}, is_shadow{};
                file.ReadBytes(&offset, sizeof(u64));
                file.ReadBytes(&index, sizeof(u64));
                file.ReadBytes(&type, sizeof(u32));
                file.ReadBytes(&is_array, sizeof(u8));
                file.ReadBytes(&is_shadow, sizeof(u8));

                entry.entries.samplers.emplace_back(
                    static_cast<std::size_t>(offset), static_cast<std::size_t>(index),
                    static_cast<Tegra::Shader::TextureType>(type), is_array != 0, is_shadow != 0);
            }

            u32 global_memory_count{};
            file.ReadBytes(&global_memory_count, sizeof(u32));
            for (u32 i = 0; i < global_memory_count; ++i) {
                u32 cbuf_index{}, cbuf_offset{};
                file.ReadBytes(&cbuf_index, sizeof(u32));
                file.ReadBytes(&cbuf_offset, sizeof(u32));
                entry.entries.global_memory_entries.emplace_back(cbuf_index, cbuf_offset);
            }

            for (auto& clip_distance : entry.entries.clip_distances) {
                u8 clip_distance_raw{};
                file.ReadBytes(&clip_distance_raw, sizeof(u8));
                clip_distance = clip_distance_raw != 0;
            }

            u64 shader_length{};
            file.ReadBytes(&shader_length, sizeof(u64));
            entry.entries.shader_length = static_cast<std::size_t>(shader_length);

            decompiled.insert({unique_identifier, std::move(entry)});
            break;
        }
        case PrecompiledEntryKind::Dump: {
            ShaderDiskCacheUsage usage;
            file.ReadBytes(&usage, sizeof(usage));

            ShaderDiskCacheDump dump;
            file.ReadBytes(&dump.binary_format, sizeof(u32));

            u32 binary_length{};
            u32 compressed_size{};
            file.ReadBytes(&binary_length, sizeof(u32));
            file.ReadBytes(&compressed_size, sizeof(u32));

            std::vector<u8> compressed_binary(compressed_size);
            file.ReadArray(compressed_binary.data(), compressed_binary.size());

            dump.binary = DecompressData(compressed_binary, binary_length);
            if (dump.binary.empty()) {
                LOG_ERROR(Render_OpenGL,
                          "Failed to decompress precompiled binary program - removing");
                InvalidatePrecompiled();
                dumps.clear();
                decompiled.clear();
                return false;
            }

            dumps.insert({usage, dump});
            break;
        }
        default:
            LOG_ERROR(Render_OpenGL, "Unknown precompiled shader cache entry kind={} - removing",
                      static_cast<u32>(kind));
            InvalidatePrecompiled();
            dumps.clear();
            decompiled.clear();
            return false;
        }
    }
    return true;
}

void ShaderDiskCacheOpenGL::InvalidateTransferable() const {
    FileUtil::Delete(GetTransferablePath());
    InvalidatePrecompiled();
}

void ShaderDiskCacheOpenGL::InvalidatePrecompiled() const {
    FileUtil::Delete(GetPrecompiledPath());
}

void ShaderDiskCacheOpenGL::SaveRaw(const ShaderDiskCacheRaw& entry) {
    if (!Settings::values.use_disk_shader_cache) {
        return;
    }

    const u64 id = entry.GetUniqueIdentifier();
    if (transferable.find(id) != transferable.end()) {
        // The shader already exists
        return;
    }

    FileUtil::IOFile file = AppendTransferableFile();
    if (!file.IsOpen()) {
        return;
    }
    file.WriteObject(TransferableEntryKind::Raw);
    entry.Save(file);

    transferable.insert({id, {}});
}

void ShaderDiskCacheOpenGL::SaveUsage(const ShaderDiskCacheUsage& usage) {
    if (!Settings::values.use_disk_shader_cache) {
        return;
    }

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
    file.WriteObject(TransferableEntryKind::Usage);
    file.WriteObject(usage);
}

void ShaderDiskCacheOpenGL::SaveDecompiled(u64 unique_identifier, const std::string& code,
                                           const GLShader::ShaderEntries& entries) {
    if (!Settings::values.use_disk_shader_cache) {
        return;
    }

    FileUtil::IOFile file = AppendPrecompiledFile();
    if (!file.IsOpen()) {
        return;
    }

    const std::vector<u8> compressed_code{CompressData(code.data(), code.size())};
    if (compressed_code.empty()) {
        LOG_ERROR(Render_OpenGL, "Failed to compress GLSL code - skipping shader {:016x}",
                  unique_identifier);
        return;
    }

    file.WriteObject(static_cast<u32>(PrecompiledEntryKind::Decompiled));

    file.WriteObject(unique_identifier);

    file.WriteObject(static_cast<u32>(code.size()));
    file.WriteObject(static_cast<u32>(compressed_code.size()));
    file.WriteArray(compressed_code.data(), compressed_code.size());

    file.WriteObject(static_cast<u32>(entries.const_buffers.size()));
    for (const auto& cbuf : entries.const_buffers) {
        file.WriteObject(static_cast<u32>(cbuf.GetMaxOffset()));
        file.WriteObject(static_cast<u32>(cbuf.GetIndex()));
        file.WriteObject(static_cast<u8>(cbuf.IsIndirect() ? 1 : 0));
    }

    file.WriteObject(static_cast<u32>(entries.samplers.size()));
    for (const auto& sampler : entries.samplers) {
        file.WriteObject(static_cast<u64>(sampler.GetOffset()));
        file.WriteObject(static_cast<u64>(sampler.GetIndex()));
        file.WriteObject(static_cast<u32>(sampler.GetType()));
        file.WriteObject(static_cast<u8>(sampler.IsArray() ? 1 : 0));
        file.WriteObject(static_cast<u8>(sampler.IsShadow() ? 1 : 0));
    }

    file.WriteObject(static_cast<u32>(entries.global_memory_entries.size()));
    for (const auto& gmem : entries.global_memory_entries) {
        file.WriteObject(static_cast<u32>(gmem.GetCbufIndex()));
        file.WriteObject(static_cast<u32>(gmem.GetCbufOffset()));
    }

    for (const bool clip_distance : entries.clip_distances) {
        file.WriteObject(static_cast<u8>(clip_distance ? 1 : 0));
    }

    file.WriteObject(static_cast<u64>(entries.shader_length));
}

void ShaderDiskCacheOpenGL::SaveDump(const ShaderDiskCacheUsage& usage, GLuint program) {
    if (!Settings::values.use_disk_shader_cache) {
        return;
    }

    FileUtil::IOFile file = AppendPrecompiledFile();
    if (!file.IsOpen()) {
        return;
    }

    GLint binary_length{};
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binary_length);

    GLenum binary_format{};
    std::vector<u8> binary(binary_length);
    glGetProgramBinary(program, binary_length, nullptr, &binary_format, binary.data());

    const std::vector<u8> compressed_binary = CompressData(binary.data(), binary.size());
    if (compressed_binary.empty()) {
        LOG_ERROR(Render_OpenGL, "Failed to compress binary program in shader={:016x}",
                  usage.unique_identifier);
        return;
    }

    file.WriteObject(static_cast<u32>(PrecompiledEntryKind::Dump));

    file.WriteObject(usage);

    file.WriteObject(static_cast<u32>(binary_format));
    file.WriteObject(static_cast<u32>(binary_length));
    file.WriteObject(static_cast<u32>(compressed_binary.size()));
    file.WriteArray(compressed_binary.data(), compressed_binary.size());
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