// Copyright 2019 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <fmt/format.h>

#include "common/assert.h"
#include "common/common_paths.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"

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

// Making sure sizes doesn't change by accident
static_assert(sizeof(BaseBindings) == 12);
static_assert(sizeof(ShaderDiskCacheUsage) == 24);

namespace {
std::string GetTitleID() {
    return fmt::format("{:016X}", Core::CurrentProcess()->GetTitleID());
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