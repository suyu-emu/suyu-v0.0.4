// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <vector>
#include "audio_core/common.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace AudioCore {
enum class EffectType : u8 {
    Invalid = 0,
    BufferMixer = 1,
    Aux = 2,
    Delay = 3,
    Reverb = 4,
    I3dl2Reverb = 5,
    BiquadFilter = 6,
};

enum class UsageStatus : u8 {
    Invalid = 0,
    New = 1,
    Initialized = 2,
    Used = 3,
    Removed = 4,
};

struct BufferMixerParams {
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> input{};
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> output{};
    std::array<float_le, AudioCommon::MAX_MIX_BUFFERS> volume{};
    s32_le count{};
};
static_assert(sizeof(BufferMixerParams) == 0x94, "BufferMixerParams is an invalid size");

struct AuxInfo {
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> input_mix_buffers{};
    std::array<s8, AudioCommon::MAX_MIX_BUFFERS> output_mix_buffers{};
    u32_le count{};
    s32_le sample_rate{};
    s32_le sample_count{};
    s32_le mix_buffer_count{};
    u64_le send_buffer_info{};
    u64_le send_buffer_base{};

    u64_le return_buffer_info{};
    u64_le return_buffer_base{};
};
static_assert(sizeof(AuxInfo) == 0x60, "AuxInfo is an invalid size");

class EffectInfo {
public:
    struct InParams {
        EffectType type{};
        u8 is_new{};
        u8 is_enabled{};
        INSERT_PADDING_BYTES(1);
        s32_le mix_id{};
        u64_le buffer_address{};
        u64_le buffer_size{};
        s32_le priority{};
        INSERT_PADDING_BYTES(4);
        union {
            std::array<u8, 0xa0> raw;
        };
    };
    static_assert(sizeof(EffectInfo::InParams) == 0xc0, "InParams is an invalid size");

    struct OutParams {
        UsageStatus status{};
        INSERT_PADDING_BYTES(15);
    };
    static_assert(sizeof(EffectInfo::OutParams) == 0x10, "OutParams is an invalid size");
};

class EffectBase {
public:
    EffectBase();
    ~EffectBase();

    virtual void Update(EffectInfo::InParams& in_params) = 0;
    UsageStatus GetUsage() const;

protected:
    UsageStatus usage{UsageStatus::Invalid};
};

class EffectStubbed : public EffectBase {
public:
    explicit EffectStubbed();
    ~EffectStubbed();

    void Update(EffectInfo::InParams& in_params) override;
};

class EffectContext {
public:
    explicit EffectContext(std::size_t effect_count);
    ~EffectContext();

    std::size_t GetCount() const;
    EffectBase* GetInfo(std::size_t i);
    const EffectBase* GetInfo(std::size_t i) const;

private:
    std::size_t effect_count{};
    std::vector<std::unique_ptr<EffectBase>> effects;
};
} // namespace AudioCore
