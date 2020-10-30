// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"
#include "video_core/command_classes/nvdec_common.h"

extern "C" {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <libavcodec/avcodec.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

namespace Tegra {
class GPU;
struct VicRegisters;

namespace Decoder {
class H264;
class VP9;
} // namespace Decoder

class Codec {
public:
    explicit Codec(GPU& gpu);
    ~Codec();

    /// Sets NVDEC video stream codec
    void SetTargetCodec(NvdecCommon::VideoCodec codec);

    /// Populate NvdecRegisters state with argument value at the provided offset
    void StateWrite(u32 offset, u64 arguments);

    /// Call decoders to construct headers, decode AVFrame with ffmpeg
    void Decode();

    /// Returns most recently decoded frame
    [[nodiscard]] AVFrame* GetCurrentFrame();
    [[nodiscard]] const AVFrame* GetCurrentFrame() const;

    /// Returns the value of current_codec
    [[nodiscard]] NvdecCommon::VideoCodec GetCurrentCodec() const;

private:
    bool initialized{};
    NvdecCommon::VideoCodec current_codec{NvdecCommon::VideoCodec::None};

    AVCodec* av_codec{nullptr};
    AVCodecContext* av_codec_ctx{nullptr};
    AVFrame* av_frame{nullptr};

    GPU& gpu;
    std::unique_ptr<Decoder::H264> h264_decoder;
    std::unique_ptr<Decoder::VP9> vp9_decoder;

    NvdecCommon::NvdecRegisters state{};
};

} // namespace Tegra
