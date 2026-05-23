// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdlib>
#include <sys/stat.h>
#include "shader_recompiler/environment.h"

class FileEnvironment final : public Shader::Environment {
public:
    FileEnvironment() = default;
    ~FileEnvironment() override = default;
    FileEnvironment& operator=(FileEnvironment&&) noexcept = default;
    FileEnvironment(FileEnvironment&&) noexcept = default;
    FileEnvironment& operator=(const FileEnvironment&) = delete;
    FileEnvironment(const FileEnvironment&) = delete;
    void Deserialize(std::ifstream& file) {}
    [[nodiscard]] u64 ReadInstruction(u32 address) override {
        if (address < read_lowest || address > read_highest) {
            std::printf("cant read %08x\n", address);
            std::abort();
        }
        return code[(address - read_lowest) / sizeof(u64)];
    }
    [[nodiscard]] u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) override { return 0; }
    [[nodiscard]] Shader::TextureType ReadTextureType(u32 handle) override {
        auto const it{texture_types.find(handle)};
        return it->second;
    }
    [[nodiscard]] Shader::TexturePixelFormat ReadTexturePixelFormat(u32 handle) override {
        auto const it{texture_pixel_formats.find(handle)};
        return it->second;
    }
    [[nodiscard]] bool IsTexturePixelFormatInteger(u32 handle) override { return true; }
    [[nodiscard]] u32 ReadViewportTransformState() override { return viewport_transform_state; }
    [[nodiscard]] u32 LocalMemorySize() const override { return local_memory_size; }
    [[nodiscard]] u32 SharedMemorySize() const override { return shared_memory_size; }
    [[nodiscard]] u32 TextureBoundBuffer() const override { return texture_bound; }
    [[nodiscard]] std::array<u32, 3> WorkgroupSize() const override { return workgroup_size; }
    [[nodiscard]] std::optional<Shader::ReplaceConstant> GetReplaceConstBuffer(u32 bank, u32 offset) override {
        auto const it = cbuf_replacements.find((u64(bank) << 32) | u64(offset));
        return it != cbuf_replacements.end() ? std::optional{it->second} : std::nullopt;
    }
    [[nodiscard]] bool HasHLEMacroState() const override { return cbuf_replacements.size() != 0; }
    void Dump(u64 pipeline_hash, u64 shader_hash) override {}
    std::vector<u64> code;
    std::unordered_map<u32, Shader::TextureType> texture_types;
    std::unordered_map<u32, Shader::TexturePixelFormat> texture_pixel_formats;
    std::unordered_map<u64, u32> cbuf_values;
    std::unordered_map<u64, Shader::ReplaceConstant> cbuf_replacements;
    std::array<u32, 3> workgroup_size{};
    u32 local_memory_size{};
    u32 shared_memory_size{};
    u32 texture_bound{};
    u32 read_lowest{};
    u32 read_highest{};
    u32 initial_offset{};
    u32 viewport_transform_state = 1;
};
