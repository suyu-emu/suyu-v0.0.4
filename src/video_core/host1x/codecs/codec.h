// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <string_view>
#include <queue>
#include "common/common_types.h"
#include "video_core/host1x/nvdec_common.h"

extern "C" {
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#endif
#include <libavcodec/avcodec.h>
#include <libavfilter/avfilter.h>
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif
}

namespace Tegra {

void AVFrameDeleter(AVFrame* ptr);
using AVFramePtr = std::unique_ptr<AVFrame, decltype(&AVFrameDeleter)>;

namespace Decoder {
class H264;
class VP8;
class VP9;
} // namespace Decoder

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
    [[nodiscard]] AVFramePtr GetCurrentFrame();

    /// Returns the value of current_codec
    [[nodiscard]] Host1x::NvdecCommon::VideoCodec GetCurrentCodec() const;

    /// Return name of the current codec
    [[nodiscard]] std::string_view GetCurrentCodecName() const;

private:
    void InitializeAvCodecContext();

    void InitializeAvFilters(AVFrame* frame);

    void InitializeGpuDecoder();

    bool CreateGpuAvDevice();

    bool initialized{};
    bool filters_initialized{};
    Host1x::NvdecCommon::VideoCodec current_codec{Host1x::NvdecCommon::VideoCodec::None};

    const AVCodec* av_codec{nullptr};
    AVCodecContext* av_codec_ctx{nullptr};
    AVBufferRef* av_gpu_decoder{nullptr};

    AVFilterContext* av_filter_src_ctx{nullptr};
    AVFilterContext* av_filter_sink_ctx{nullptr};
    AVFilterGraph* av_filter_graph{nullptr};

    Host1x::Host1x& host1x;
    const Host1x::NvdecCommon::NvdecRegisters& state;
    std::unique_ptr<Decoder::H264> h264_decoder;
    std::unique_ptr<Decoder::VP8> vp8_decoder;
    std::unique_ptr<Decoder::VP9> vp9_decoder;

    std::queue<AVFramePtr> av_frames{};
};

} // namespace Tegra
