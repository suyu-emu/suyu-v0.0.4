// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include <fmt/format.h>

#include "common/assert.h"
#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/zstd_compression.h"
#include "core/core.h"
#include "core/hle/kernel/process.h"
#include "core/settings.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_shader_cache.h"
#include "video_core/renderer_opengl/gl_shader_disk_cache.h"

namespace OpenGL {

using Tegra::Engines::ShaderType;
using VideoCommon::Shader::BindlessSamplerMap;
using VideoCommon::Shader::BoundSamplerMap;
using VideoCommon::Shader::KeyMap;

namespace {

using ShaderCacheVersionHash = std::array<u8, 64>;

enum class TransferableEntryKind : u32 {
    Raw,
    Usage,
};

struct ConstBufferKey {
    u32 cbuf{};
    u32 offset{};
    u32 value{};
};

struct BoundSamplerKey {
    u32 offset{};
    Tegra::Engines::SamplerDescriptor sampler{};
};

struct BindlessSamplerKey {
    u32 cbuf{};
    u32 offset{};
    Tegra::Engines::SamplerDescriptor sampler{};
};

constexpr u32 NativeVersion = 11;

// Making sure sizes doesn't change by accident
static_assert(sizeof(ProgramVariant) == 20);

ShaderCacheVersionHash GetShaderCacheVersionHash() {
    ShaderCacheVersionHash hash{};
    const std::size_t length = std::min(std::strlen(Common::g_shader_cache_version), hash.size());
    std::memcpy(hash.data(), Common::g_shader_cache_version, length);
    return hash;
}

} // Anonymous namespace

ShaderDiskCacheRaw::ShaderDiskCacheRaw(u64 unique_identifier, ShaderType type, ProgramCode code,
                                       ProgramCode code_b)
    : unique_identifier{unique_identifier}, type{type}, code{std::move(code)}, code_b{std::move(
                                                                                   code_b)} {}

ShaderDiskCacheRaw::ShaderDiskCacheRaw() = default;

ShaderDiskCacheRaw::~ShaderDiskCacheRaw() = default;

bool ShaderDiskCacheRaw::Load(FileUtil::IOFile& file) {
    if (file.ReadBytes(&unique_identifier, sizeof(u64)) != sizeof(u64) ||
        file.ReadBytes(&type, sizeof(u32)) != sizeof(u32)) {
        return false;
    }
    u32 code_size{};
    u32 code_size_b{};
    if (file.ReadBytes(&code_size, sizeof(u32)) != sizeof(u32) ||
        file.ReadBytes(&code_size_b, sizeof(u32)) != sizeof(u32)) {
        return false;
    }

    code.resize(code_size);
    code_b.resize(code_size_b);

    if (file.ReadArray(code.data(), code_size) != code_size)
        return false;

    if (HasProgramA() && file.ReadArray(code_b.data(), code_size_b) != code_size_b) {
        return false;
    }
    return true;
}

bool ShaderDiskCacheRaw::Save(FileUtil::IOFile& file) const {
    if (file.WriteObject(unique_identifier) != 1 || file.WriteObject(static_cast<u32>(type)) != 1 ||
        file.WriteObject(static_cast<u32>(code.size())) != 1 ||
        file.WriteObject(static_cast<u32>(code_b.size())) != 1) {
        return false;
    }

    if (file.WriteArray(code.data(), code.size()) != code.size())
        return false;

    if (HasProgramA() && file.WriteArray(code_b.data(), code_b.size()) != code_b.size()) {
        return false;
    }
    return true;
}

ShaderDiskCacheOpenGL::ShaderDiskCacheOpenGL(Core::System& system) : system{system} {}

ShaderDiskCacheOpenGL::~ShaderDiskCacheOpenGL() = default;

std::optional<std::pair<std::vector<ShaderDiskCacheRaw>, std::vector<ShaderDiskCacheUsage>>>
ShaderDiskCacheOpenGL::LoadTransferable() {
    // Skip games without title id
    const bool has_title_id = system.CurrentProcess()->GetTitleID() != 0;
    if (!Settings::values.use_disk_shader_cache || !has_title_id) {
        return {};
    }

    FileUtil::IOFile file(GetTransferablePath(), "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No transferable shader cache found for game with title id={}",
                 GetTitleID());
        is_usable = true;
        return {};
    }

    u32 version{};
    if (file.ReadBytes(&version, sizeof(version)) != sizeof(version)) {
        LOG_ERROR(Render_OpenGL,
                  "Failed to get transferable cache version for title id={}, skipping",
                  GetTitleID());
        return {};
    }

    if (version < NativeVersion) {
        LOG_INFO(Render_OpenGL, "Transferable shader cache is old, removing");
        file.Close();
        InvalidateTransferable();
        is_usable = true;
        return {};
    }
    if (version > NativeVersion) {
        LOG_WARNING(Render_OpenGL, "Transferable shader cache was generated with a newer version "
                                   "of the emulator, skipping");
        return {};
    }

    // Version is valid, load the shaders
    constexpr const char error_loading[] = "Failed to load transferable raw entry, skipping";
    std::vector<ShaderDiskCacheRaw> raws;
    std::vector<ShaderDiskCacheUsage> usages;
    while (file.Tell() < file.GetSize()) {
        TransferableEntryKind kind{};
        if (file.ReadBytes(&kind, sizeof(u32)) != sizeof(u32)) {
            LOG_ERROR(Render_OpenGL, "Failed to read transferable file, skipping");
            return {};
        }

        switch (kind) {
        case TransferableEntryKind::Raw: {
            ShaderDiskCacheRaw entry;
            if (!entry.Load(file)) {
                LOG_ERROR(Render_OpenGL, error_loading);
                return {};
            }
            transferable.insert({entry.GetUniqueIdentifier(), {}});
            raws.push_back(std::move(entry));
            break;
        }
        case TransferableEntryKind::Usage: {
            ShaderDiskCacheUsage usage;

            u32 num_keys{};
            u32 num_bound_samplers{};
            u32 num_bindless_samplers{};
            if (file.ReadArray(&usage.unique_identifier, 1) != 1 ||
                file.ReadArray(&usage.variant, 1) != 1 || file.ReadArray(&num_keys, 1) != 1 ||
                file.ReadArray(&num_bound_samplers, 1) != 1 ||
                file.ReadArray(&num_bindless_samplers, 1) != 1) {
                LOG_ERROR(Render_OpenGL, error_loading);
                return {};
            }

            std::vector<ConstBufferKey> keys(num_keys);
            std::vector<BoundSamplerKey> bound_samplers(num_bound_samplers);
            std::vector<BindlessSamplerKey> bindless_samplers(num_bindless_samplers);
            if (file.ReadArray(keys.data(), keys.size()) != keys.size() ||
                file.ReadArray(bound_samplers.data(), bound_samplers.size()) !=
                    bound_samplers.size() ||
                file.ReadArray(bindless_samplers.data(), bindless_samplers.size()) !=
                    bindless_samplers.size()) {
                LOG_ERROR(Render_OpenGL, error_loading);
                return {};
            }
            for (const auto& key : keys) {
                usage.keys.insert({{key.cbuf, key.offset}, key.value});
            }
            for (const auto& key : bound_samplers) {
                usage.bound_samplers.emplace(key.offset, key.sampler);
            }
            for (const auto& key : bindless_samplers) {
                usage.bindless_samplers.insert({{key.cbuf, key.offset}, key.sampler});
            }

            usages.push_back(std::move(usage));
            break;
        }
        default:
            LOG_ERROR(Render_OpenGL, "Unknown transferable shader cache entry kind={}, skipping",
                      static_cast<u32>(kind));
            return {};
        }
    }

    is_usable = true;
    return {{std::move(raws), std::move(usages)}};
}

std::unordered_map<ShaderDiskCacheUsage, ShaderDiskCacheDump>
ShaderDiskCacheOpenGL::LoadPrecompiled() {
    if (!is_usable) {
        return {};
    }

    std::string path = GetPrecompiledPath();
    FileUtil::IOFile file(path, "rb");
    if (!file.IsOpen()) {
        LOG_INFO(Render_OpenGL, "No precompiled shader cache found for game with title id={}",
                 GetTitleID());
        return {};
    }

    const auto result = LoadPrecompiledFile(file);
    if (!result) {
        LOG_INFO(Render_OpenGL,
                 "Failed to load precompiled cache for game with title id={}, removing",
                 GetTitleID());
        file.Close();
        InvalidatePrecompiled();
        return {};
    }
    return *result;
}

std::optional<std::unordered_map<ShaderDiskCacheUsage, ShaderDiskCacheDump>>
ShaderDiskCacheOpenGL::LoadPrecompiledFile(FileUtil::IOFile& file) {
    // Read compressed file from disk and decompress to virtual precompiled cache file
    std::vector<u8> compressed(file.GetSize());
    file.ReadBytes(compressed.data(), compressed.size());
    const std::vector<u8> decompressed = Common::Compression::DecompressDataZSTD(compressed);
    SaveArrayToPrecompiled(decompressed.data(), decompressed.size());
    precompiled_cache_virtual_file_offset = 0;

    ShaderCacheVersionHash file_hash{};
    if (!LoadArrayFromPrecompiled(file_hash.data(), file_hash.size())) {
        precompiled_cache_virtual_file_offset = 0;
        return {};
    }
    if (GetShaderCacheVersionHash() != file_hash) {
        LOG_INFO(Render_OpenGL, "Precompiled cache is from another version of the emulator");
        precompiled_cache_virtual_file_offset = 0;
        return {};
    }

    ShaderDumpsMap dumps;
    while (precompiled_cache_virtual_file_offset < precompiled_cache_virtual_file.GetSize()) {
        u32 num_keys{};
        u32 num_bound_samplers{};
        u32 num_bindless_samplers{};
        ShaderDiskCacheUsage usage;
        if (!LoadObjectFromPrecompiled(usage.unique_identifier) ||
            !LoadObjectFromPrecompiled(usage.variant) || !LoadObjectFromPrecompiled(num_keys) ||
            !LoadObjectFromPrecompiled(num_bound_samplers) ||
            !LoadObjectFromPrecompiled(num_bindless_samplers)) {
            return {};
        }
        std::vector<ConstBufferKey> keys(num_keys);
        std::vector<BoundSamplerKey> bound_samplers(num_bound_samplers);
        std::vector<BindlessSamplerKey> bindless_samplers(num_bindless_samplers);
        if (!LoadArrayFromPrecompiled(keys.data(), keys.size()) ||
            !LoadArrayFromPrecompiled(bound_samplers.data(), bound_samplers.size()) !=
                bound_samplers.size() ||
            !LoadArrayFromPrecompiled(bindless_samplers.data(), bindless_samplers.size()) !=
                bindless_samplers.size()) {
            return {};
        }
        for (const auto& key : keys) {
            usage.keys.insert({{key.cbuf, key.offset}, key.value});
        }
        for (const auto& key : bound_samplers) {
            usage.bound_samplers.emplace(key.offset, key.sampler);
        }
        for (const auto& key : bindless_samplers) {
            usage.bindless_samplers.insert({{key.cbuf, key.offset}, key.sampler});
        }

        ShaderDiskCacheDump dump;
        if (!LoadObjectFromPrecompiled(dump.binary_format)) {
            return {};
        }

        u32 binary_length{};
        if (!LoadObjectFromPrecompiled(binary_length)) {
            return {};
        }

        dump.binary.resize(binary_length);
        if (!LoadArrayFromPrecompiled(dump.binary.data(), dump.binary.size())) {
            return {};
        }

        dumps.emplace(std::move(usage), dump);
    }
    return dumps;
}

void ShaderDiskCacheOpenGL::InvalidateTransferable() {
    if (!FileUtil::Delete(GetTransferablePath())) {
        LOG_ERROR(Render_OpenGL, "Failed to invalidate transferable file={}",
                  GetTransferablePath());
    }
    InvalidatePrecompiled();
}

void ShaderDiskCacheOpenGL::InvalidatePrecompiled() {
    // Clear virtaul precompiled cache file
    precompiled_cache_virtual_file.Resize(0);

    if (!FileUtil::Delete(GetPrecompiledPath())) {
        LOG_ERROR(Render_OpenGL, "Failed to invalidate precompiled file={}", GetPrecompiledPath());
    }
}

void ShaderDiskCacheOpenGL::SaveRaw(const ShaderDiskCacheRaw& entry) {
    if (!is_usable) {
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
    if (file.WriteObject(TransferableEntryKind::Raw) != 1 || !entry.Save(file)) {
        LOG_ERROR(Render_OpenGL, "Failed to save raw transferable cache entry, removing");
        file.Close();
        InvalidateTransferable();
        return;
    }
    transferable.insert({id, {}});
}

void ShaderDiskCacheOpenGL::SaveUsage(const ShaderDiskCacheUsage& usage) {
    if (!is_usable) {
        return;
    }

    const auto it = transferable.find(usage.unique_identifier);
    ASSERT_MSG(it != transferable.end(), "Saving shader usage without storing raw previously");

    auto& usages{it->second};
    if (usages.find(usage) != usages.end()) {
        // Skip this variant since the shader is already stored.
        return;
    }
    usages.insert(usage);

    FileUtil::IOFile file = AppendTransferableFile();
    if (!file.IsOpen())
        return;
    const auto Close = [&] {
        LOG_ERROR(Render_OpenGL, "Failed to save usage transferable cache entry, removing");
        file.Close();
        InvalidateTransferable();
    };

    if (file.WriteObject(TransferableEntryKind::Usage) != 1 ||
        file.WriteObject(usage.unique_identifier) != 1 || file.WriteObject(usage.variant) != 1 ||
        file.WriteObject(static_cast<u32>(usage.keys.size())) != 1 ||
        file.WriteObject(static_cast<u32>(usage.bound_samplers.size())) != 1 ||
        file.WriteObject(static_cast<u32>(usage.bindless_samplers.size())) != 1) {
        Close();
        return;
    }
    for (const auto& [pair, value] : usage.keys) {
        const auto [cbuf, offset] = pair;
        if (file.WriteObject(ConstBufferKey{cbuf, offset, value}) != 1) {
            Close();
            return;
        }
    }
    for (const auto& [offset, sampler] : usage.bound_samplers) {
        if (file.WriteObject(BoundSamplerKey{offset, sampler}) != 1) {
            Close();
            return;
        }
    }
    for (const auto& [pair, sampler] : usage.bindless_samplers) {
        const auto [cbuf, offset] = pair;
        if (file.WriteObject(BindlessSamplerKey{cbuf, offset, sampler}) != 1) {
            Close();
            return;
        }
    }
}

void ShaderDiskCacheOpenGL::SaveDump(const ShaderDiskCacheUsage& usage, GLuint program) {
    if (!is_usable) {
        return;
    }

    // TODO(Rodrigo): This is a design smell. I shouldn't be having to manually write the header
    // when writing the dump. This should be done the moment I get access to write to the virtual
    // file.
    if (precompiled_cache_virtual_file.GetSize() == 0) {
        SavePrecompiledHeaderToVirtualPrecompiledCache();
    }

    GLint binary_length{};
    glGetProgramiv(program, GL_PROGRAM_BINARY_LENGTH, &binary_length);

    GLenum binary_format{};
    std::vector<u8> binary(binary_length);
    glGetProgramBinary(program, binary_length, nullptr, &binary_format, binary.data());

    const auto Close = [&] {
        LOG_ERROR(Render_OpenGL, "Failed to save binary program file in shader={:016X}, removing",
                  usage.unique_identifier);
        InvalidatePrecompiled();
    };

    if (!SaveObjectToPrecompiled(usage.unique_identifier) ||
        !SaveObjectToPrecompiled(usage.variant) ||
        !SaveObjectToPrecompiled(static_cast<u32>(usage.keys.size())) ||
        !SaveObjectToPrecompiled(static_cast<u32>(usage.bound_samplers.size())) ||
        !SaveObjectToPrecompiled(static_cast<u32>(usage.bindless_samplers.size()))) {
        Close();
        return;
    }
    for (const auto& [pair, value] : usage.keys) {
        const auto [cbuf, offset] = pair;
        if (SaveObjectToPrecompiled(ConstBufferKey{cbuf, offset, value}) != 1) {
            Close();
            return;
        }
    }
    for (const auto& [offset, sampler] : usage.bound_samplers) {
        if (SaveObjectToPrecompiled(BoundSamplerKey{offset, sampler}) != 1) {
            Close();
            return;
        }
    }
    for (const auto& [pair, sampler] : usage.bindless_samplers) {
        const auto [cbuf, offset] = pair;
        if (SaveObjectToPrecompiled(BindlessSamplerKey{cbuf, offset, sampler}) != 1) {
            Close();
            return;
        }
    }
    if (!SaveObjectToPrecompiled(static_cast<u32>(binary_format)) ||
        !SaveObjectToPrecompiled(static_cast<u32>(binary_length)) ||
        !SaveArrayToPrecompiled(binary.data(), binary.size())) {
        Close();
    }
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
        if (file.WriteObject(NativeVersion) != 1) {
            LOG_ERROR(Render_OpenGL, "Failed to write transferable cache version in path={}",
                      transferable_path);
            return {};
        }
    }
    return file;
}

void ShaderDiskCacheOpenGL::SavePrecompiledHeaderToVirtualPrecompiledCache() {
    const auto hash{GetShaderCacheVersionHash()};
    if (!SaveArrayToPrecompiled(hash.data(), hash.size())) {
        LOG_ERROR(
            Render_OpenGL,
            "Failed to write precompiled cache version hash to virtual precompiled cache file");
    }
}

void ShaderDiskCacheOpenGL::SaveVirtualPrecompiledFile() {
    precompiled_cache_virtual_file_offset = 0;
    const std::vector<u8> uncompressed = precompiled_cache_virtual_file.ReadAllBytes();
    const std::vector<u8> compressed =
        Common::Compression::CompressDataZSTDDefault(uncompressed.data(), uncompressed.size());

    const auto precompiled_path{GetPrecompiledPath()};
    FileUtil::IOFile file(precompiled_path, "wb");

    if (!file.IsOpen()) {
        LOG_ERROR(Render_OpenGL, "Failed to open precompiled cache in path={}", precompiled_path);
        return;
    }
    if (file.WriteBytes(compressed.data(), compressed.size()) != compressed.size()) {
        LOG_ERROR(Render_OpenGL, "Failed to write precompiled cache version in path={}",
                  precompiled_path);
        return;
    }
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

std::string ShaderDiskCacheOpenGL::GetTitleID() const {
    return fmt::format("{:016X}", system.CurrentProcess()->GetTitleID());
}

} // namespace OpenGL
