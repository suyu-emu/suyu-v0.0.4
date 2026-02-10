// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <ankerl/unordered_dense.h>

#include "common/common_types.h"
#include "shader_recompiler/program_header.h"
#include "shader_recompiler/shader_info.h"
#include "shader_recompiler/stage.h"
#include "shader_recompiler/frontend/ir/value.h"

namespace Shader::IR {
class Inst;
}

namespace Shader {

struct CbufWordKey {
    u32 index;
    u32 offset;
    constexpr bool operator==(const CbufWordKey& o) const noexcept {
        return index == o.index && offset == o.offset;
    }
};

struct CbufWordKeyHash {
    constexpr size_t operator()(const CbufWordKey& k) const noexcept {
        return (size_t(k.index) << 32) ^ k.offset;
    }
};

struct HandleKey {
    u32 index, offset, shift_left;
    u32 sec_index, sec_offset, sec_shift_left;
    bool has_secondary;
    constexpr bool operator==(const HandleKey& o) const noexcept {
        return std::tie(index, offset, shift_left, sec_index, sec_offset, sec_shift_left, has_secondary)
            == std::tie(o.index, o.offset, o.shift_left, o.sec_index, o.sec_offset, o.sec_shift_left, o.has_secondary);
    }
};
struct HandleKeyHash {
    constexpr size_t operator()(const HandleKey& k) const noexcept {
        size_t h = (size_t(k.index) << 32) ^ k.offset;
        h ^= (size_t(k.shift_left) << 1);
        h ^= (size_t(k.sec_index) << 33) ^ (size_t(k.sec_offset) << 2);
        h ^= (size_t(k.sec_shift_left) << 3);
        h ^= k.has_secondary ? 0x9e3779b97f4a7c15ULL : 0ULL;
        return h;
    }
};

struct ConstBufferAddr {
    u32 index;
    u32 offset;
    u32 shift_left;
    u32 secondary_index;
    u32 secondary_offset;
    u32 secondary_shift_left;
    IR::U32 dynamic_offset;
    u32 count;
    bool has_secondary;
};

class Environment {
public:
    virtual ~Environment() = default;

    [[nodiscard]] virtual u64 ReadInstruction(u32 address) = 0;

    [[nodiscard]] virtual u32 ReadCbufValue(u32 cbuf_index, u32 cbuf_offset) = 0;

    [[nodiscard]] virtual TextureType ReadTextureType(u32 raw_handle) = 0;

    [[nodiscard]] virtual TexturePixelFormat ReadTexturePixelFormat(u32 raw_handle) = 0;

    [[nodiscard]] virtual bool IsTexturePixelFormatInteger(u32 raw_handle) = 0;

    [[nodiscard]] virtual u32 ReadViewportTransformState() = 0;

    [[nodiscard]] virtual u32 TextureBoundBuffer() const = 0;

    [[nodiscard]] virtual u32 LocalMemorySize() const = 0;

    [[nodiscard]] virtual u32 SharedMemorySize() const = 0;

    [[nodiscard]] virtual std::array<u32, 3> WorkgroupSize() const = 0;

    [[nodiscard]] virtual bool HasHLEMacroState() const = 0;

    [[nodiscard]] virtual std::optional<ReplaceConstant> GetReplaceConstBuffer(u32 bank,
                                                                               u32 offset) = 0;

    virtual void Dump(u64 pipeline_hash, u64 shader_hash) = 0;

    [[nodiscard]] const ProgramHeader& SPH() const noexcept {
        return sph;
    }

    [[nodiscard]] const std::array<u32, 8>& GpPassthroughMask() const noexcept {
        return gp_passthrough_mask;
    }

    [[nodiscard]] Stage ShaderStage() const noexcept {
        return stage;
    }

    [[nodiscard]] u32 StartAddress() const noexcept {
        return start_address;
    }

    [[nodiscard]] bool IsProprietaryDriver() const noexcept {
        return is_proprietary_driver;
    }

protected:
    ProgramHeader sph{};
    std::array<u32, 8> gp_passthrough_mask{};
    Stage stage{};
    u32 start_address{};
    bool is_proprietary_driver{};
public:
    ankerl::unordered_dense::map<CbufWordKey, u32, CbufWordKeyHash> cbuf_word_cache;
    ankerl::unordered_dense::map<HandleKey,  u32, HandleKeyHash> handle_cache;
    ankerl::unordered_dense::map<const IR::Inst*, ConstBufferAddr> track_cache;
};

} // namespace Shader
