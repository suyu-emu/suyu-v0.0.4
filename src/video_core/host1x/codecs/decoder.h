// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string_view>
#include <ankerl/unordered_dense.h>
#include <queue>

#include "common/common_types.h"
#include "video_core/host1x/ffmpeg.h"
#include "video_core/host1x/nvdec_common.h"

namespace Tegra {

namespace Host1x {
class Host1x;
class FrameQueue;
} // namespace Host1x

class Decoder {
public:
    virtual ~Decoder();

    /// Call decoders to construct headers, decode AVFrame with ffmpeg
    void Decode();

    bool UsingDecodeOrder() const {
        return decode_api.UsingDecodeOrder();
    }

    /// Return name of the current codec
    [[nodiscard]] virtual std::string_view GetCurrentCodecName() const = 0;

protected:
    explicit Decoder(Host1x::Host1x& host1x, s32 id, const Host1x::NvdecCommon::NvdecRegisters& regs);

    virtual std::span<const u8> ComposeFrame() = 0;
    virtual std::tuple<u64, u64> GetProgressiveOffsets() = 0;
    virtual std::tuple<u64, u64, u64, u64> GetInterlacedOffsets() = 0;
    virtual bool IsInterlaced() = 0;

    FFmpeg::DecodeApi decode_api;
    Host1x::Host1x& host1x;
    const Host1x::NvdecCommon::NvdecRegisters& regs;
    s32 id;
    bool initialized : 1 = false;
    bool vp9_hidden_frame : 1 = false;
};

} // namespace Tegra
