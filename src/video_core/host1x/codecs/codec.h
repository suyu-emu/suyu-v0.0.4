// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <string_view>
#include <queue>
#include "common/common_types.h"
#include "video_core/host1x/codecs/h264.h"
#include "video_core/host1x/codecs/vp8.h"
#include "video_core/host1x/codecs/vp9.h"
#include "video_core/host1x/ffmpeg.h"
#include "video_core/host1x/nvdec_common.h"

namespace Tegra {

namespace Host1x {
class Host1x;
} // namespace Host1x

class Codec {
public:
    explicit Codec(Host1x::Host1x& host1x, const Host1x::NvdecCommon::NvdecRegisters& regs);
    ~Codec();

    /// Initialize the codec, returning success or failure
    void Initialize();

    /// Sets NVDEC video stream codec
    void SetTargetCodec(Host1x::NvdecCommon::VideoCodec codec);

    /// Call decoders to construct headers, decode AVFrame with ffmpeg
    void Decode();

    /// Returns next decoded frame
    [[nodiscard]] std::unique_ptr<FFmpeg::Frame> GetCurrentFrame();

    /// Return name of the current codec
    [[nodiscard]] std::string_view GetCurrentCodecName() const;

private:
    bool initialized{};
    Host1x::NvdecCommon::VideoCodec current_codec{Host1x::NvdecCommon::VideoCodec::None};
    FFmpeg::DecodeApi decode_api;

    Host1x::Host1x& host1x;
    const Host1x::NvdecCommon::NvdecRegisters& state;
    Decoders::H264 h264_decoder;
    Decoders::VP8 vp8_decoder;
    Decoders::VP9 vp9_decoder;
    std::queue<std::unique_ptr<FFmpeg::Frame>> frames{};
};

} // namespace Tegra
