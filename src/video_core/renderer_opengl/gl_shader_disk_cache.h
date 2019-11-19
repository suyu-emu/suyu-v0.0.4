// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <optional>
#include <string>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <glad/glad.h>

#include "common/assert.h"
#include "common/common_types.h"
#include "core/file_sys/vfs_vector.h"
#include "video_core/engines/shader_type.h"
#include "video_core/renderer_opengl/gl_shader_gen.h"
#include "video_core/shader/const_buffer_locker.h"

namespace Core {
class System;
}

namespace FileUtil {
class IOFile;
}

namespace OpenGL {

struct ShaderDiskCacheUsage;
struct ShaderDiskCacheDump;

using ProgramCode = std::vector<u64>;
using ShaderDumpsMap = std::unordered_map<ShaderDiskCacheUsage, ShaderDiskCacheDump>;

/// Describes the different variants a program can be compiled with.
struct ProgramVariant final {
    ProgramVariant() = default;

    /// Graphics constructor.
    explicit constexpr ProgramVariant(GLenum primitive_mode) noexcept
        : primitive_mode{primitive_mode} {}

    /// Compute constructor.
    explicit constexpr ProgramVariant(u32 block_x, u32 block_y, u32 block_z, u32 shared_memory_size,
                                      u32 local_memory_size) noexcept
        : block_x{block_x}, block_y{static_cast<u16>(block_y)}, block_z{static_cast<u16>(block_z)},
          shared_memory_size{shared_memory_size}, local_memory_size{local_memory_size} {}

    // Graphics specific parameters.
    GLenum primitive_mode{};

    // Compute specific parameters.
    u32 block_x{};
    u16 block_y{};
    u16 block_z{};
    u32 shared_memory_size{};
    u32 local_memory_size{};

    bool operator==(const ProgramVariant& rhs) const noexcept {
        return std::tie(primitive_mode, block_x, block_y, block_z, shared_memory_size,
                        local_memory_size) == std::tie(rhs.primitive_mode, rhs.block_x, rhs.block_y,
                                                       rhs.block_z, rhs.shared_memory_size,
                                                       rhs.local_memory_size);
    }

    bool operator!=(const ProgramVariant& rhs) const noexcept {
        return !operator==(rhs);
    }
};
static_assert(std::is_trivially_copyable_v<ProgramVariant>);

/// Describes how a shader is used.
struct ShaderDiskCacheUsage {
    u64 unique_identifier{};
    ProgramVariant variant;
    VideoCommon::Shader::KeyMap keys;
    VideoCommon::Shader::BoundSamplerMap bound_samplers;
    VideoCommon::Shader::BindlessSamplerMap bindless_samplers;

    bool operator==(const ShaderDiskCacheUsage& rhs) const {
        return std::tie(unique_identifier, variant, keys, bound_samplers, bindless_samplers) ==
               std::tie(rhs.unique_identifier, rhs.variant, rhs.keys, rhs.bound_samplers,
                        rhs.bindless_samplers);
    }

    bool operator!=(const ShaderDiskCacheUsage& rhs) const {
        return !operator==(rhs);
    }
};

} // namespace OpenGL

namespace std {

template <>
struct hash<OpenGL::ProgramVariant> {
    std::size_t operator()(const OpenGL::ProgramVariant& variant) const noexcept {
        return (static_cast<std::size_t>(variant.primitive_mode) << 6) ^
               static_cast<std::size_t>(variant.block_x) ^
               (static_cast<std::size_t>(variant.block_y) << 32) ^
               (static_cast<std::size_t>(variant.block_z) << 48) ^
               (static_cast<std::size_t>(variant.shared_memory_size) << 16) ^
               (static_cast<std::size_t>(variant.local_memory_size) << 36);
    }
};

template <>
struct hash<OpenGL::ShaderDiskCacheUsage> {
    std::size_t operator()(const OpenGL::ShaderDiskCacheUsage& usage) const noexcept {
        return static_cast<std::size_t>(usage.unique_identifier) ^
               std::hash<OpenGL::ProgramVariant>{}(usage.variant);
    }
};

} // namespace std

namespace OpenGL {

/// Describes a shader how it's used by the guest GPU
class ShaderDiskCacheRaw {
public:
    explicit ShaderDiskCacheRaw(u64 unique_identifier, Tegra::Engines::ShaderType type,
                                ProgramCode code, ProgramCode code_b = {});
    ShaderDiskCacheRaw();
    ~ShaderDiskCacheRaw();

    bool Load(FileUtil::IOFile& file);

    bool Save(FileUtil::IOFile& file) const;

    u64 GetUniqueIdentifier() const {
        return unique_identifier;
    }

    bool HasProgramA() const {
        return !code.empty() && !code_b.empty();
    }

    Tegra::Engines::ShaderType GetType() const {
        return type;
    }

    const ProgramCode& GetCode() const {
        return code;
    }

    const ProgramCode& GetCodeB() const {
        return code_b;
    }

private:
    u64 unique_identifier{};
    Tegra::Engines::ShaderType type{};
    ProgramCode code;
    ProgramCode code_b;
};

/// Contains an OpenGL dumped binary program
struct ShaderDiskCacheDump {
    GLenum binary_format{};
    std::vector<u8> binary;
};

class ShaderDiskCacheOpenGL {
public:
    explicit ShaderDiskCacheOpenGL(Core::System& system);
    ~ShaderDiskCacheOpenGL();

    /// Loads transferable cache. If file has a old version or on failure, it deletes the file.
    std::optional<std::pair<std::vector<ShaderDiskCacheRaw>, std::vector<ShaderDiskCacheUsage>>>
    LoadTransferable();

    /// Loads current game's precompiled cache. Invalidates on failure.
    std::unordered_map<ShaderDiskCacheUsage, ShaderDiskCacheDump> LoadPrecompiled();

    /// Removes the transferable (and precompiled) cache file.
    void InvalidateTransferable();

    /// Removes the precompiled cache file and clears virtual precompiled cache file.
    void InvalidatePrecompiled();

    /// Saves a raw dump to the transferable file. Checks for collisions.
    void SaveRaw(const ShaderDiskCacheRaw& entry);

    /// Saves shader usage to the transferable file. Does not check for collisions.
    void SaveUsage(const ShaderDiskCacheUsage& usage);

    /// Saves a dump entry to the precompiled file. Does not check for collisions.
    void SaveDump(const ShaderDiskCacheUsage& usage, GLuint program);

    /// Serializes virtual precompiled shader cache file to real file
    void SaveVirtualPrecompiledFile();

private:
    /// Loads the transferable cache. Returns empty on failure.
    std::optional<std::unordered_map<ShaderDiskCacheUsage, ShaderDiskCacheDump>>
    LoadPrecompiledFile(FileUtil::IOFile& file);

    /// Opens current game's transferable file and write it's header if it doesn't exist
    FileUtil::IOFile AppendTransferableFile() const;

    /// Save precompiled header to precompiled_cache_in_memory
    void SavePrecompiledHeaderToVirtualPrecompiledCache();

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

    /// Get current game's title id
    std::string GetTitleID() const;

    template <typename T>
    bool SaveArrayToPrecompiled(const T* data, std::size_t length) {
        const std::size_t write_length = precompiled_cache_virtual_file.WriteArray(
            data, length, precompiled_cache_virtual_file_offset);
        precompiled_cache_virtual_file_offset += write_length;
        return write_length == sizeof(T) * length;
    }

    template <typename T>
    bool LoadArrayFromPrecompiled(T* data, std::size_t length) {
        const std::size_t read_length = precompiled_cache_virtual_file.ReadArray(
            data, length, precompiled_cache_virtual_file_offset);
        precompiled_cache_virtual_file_offset += read_length;
        return read_length == sizeof(T) * length;
    }

    template <typename T>
    bool SaveObjectToPrecompiled(const T& object) {
        return SaveArrayToPrecompiled(&object, 1);
    }

    bool SaveObjectToPrecompiled(bool object) {
        const auto value = static_cast<u8>(object);
        return SaveArrayToPrecompiled(&value, 1);
    }

    template <typename T>
    bool LoadObjectFromPrecompiled(T& object) {
        return LoadArrayFromPrecompiled(&object, 1);
    }

    Core::System& system;

    // Stores whole precompiled cache which will be read from or saved to the precompiled chache
    // file
    FileSys::VectorVfsFile precompiled_cache_virtual_file;
    // Stores the current offset of the precompiled cache file for IO purposes
    std::size_t precompiled_cache_virtual_file_offset = 0;

    // Stored transferable shaders
    std::unordered_map<u64, std::unordered_set<ShaderDiskCacheUsage>> transferable;

    // The cache has been loaded at boot
    bool is_usable{};
};

} // namespace OpenGL
