// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "audio_core/common.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"

namespace AudioCore {

enum class SinkTypes : u8 {
    Invalid = 0,
    Device = 1,
    Circular = 2,
};

enum class SinkSampleFormat : u32_le {
    None = 0,
    Pcm8 = 1,
    Pcm16 = 2,
    Pcm24 = 3,
    Pcm32 = 4,
    PcmFloat = 5,
    Adpcm = 6,
};

class SinkInfo {
public:
    struct CircularBufferIn {
        u64_le address;
        u32_le size;
        u32_le input_count;
        u32_le sample_count;
        u32_le previous_position;
        SinkSampleFormat sample_format;
        std::array<u8, AudioCommon::MAX_CHANNEL_COUNT> input;
        bool in_use;
        INSERT_UNION_PADDING_BYTES(5);
    };
    static_assert(sizeof(SinkInfo::CircularBufferIn) == 0x28,
                  "SinkInfo::CircularBufferIn is in invalid size");

    struct DeviceIn {
        std::array<u8, 255> device_name;
        INSERT_UNION_PADDING_BYTES(1);
        s32_le input_count;
        std::array<u8, AudioCommon::MAX_CHANNEL_COUNT> input;
        INSERT_UNION_PADDING_BYTES(1);
        bool down_matrix_enabled;
        std::array<float_le, 4> down_matrix_coef;
    };
    static_assert(sizeof(SinkInfo::DeviceIn) == 0x11c, "SinkInfo::DeviceIn is an invalid size");

    struct InParams {
        SinkTypes type{};
        bool in_use{};
        INSERT_PADDING_BYTES(2);
        u32_le node_id{};
        INSERT_PADDING_WORDS(6);
        union {
            // std::array<u8, 0x120> raw{};
            SinkInfo::DeviceIn device;
            SinkInfo::CircularBufferIn circular_buffer;
        };
    };
    static_assert(sizeof(SinkInfo::InParams) == 0x140, "SinkInfo::InParams are an invalid size!");
};

class SinkContext {
public:
    explicit SinkContext(std::size_t sink_count);
    ~SinkContext();

    std::size_t GetCount() const;

    void UpdateMainSink(SinkInfo::InParams& in);
    bool InUse() const;
    std::vector<u8> OutputBuffers() const;

private:
    bool in_use{false};
    s32 use_count{};
    std::array<u8, AudioCommon::MAX_CHANNEL_COUNT> buffers{};
    std::size_t sink_count{};
};
} // namespace AudioCore
