// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/assert.h"
#include "common/settings.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/host1x.h"
#include "video_core/memory_manager.h"

namespace Tegra {

Decoder::Decoder(Host1x::Host1x& host1x_, s32 id_, const Host1x::NvdecCommon::NvdecRegisters& regs_)
    : host1x(host1x_)
    , regs{regs_}
    , id{id_}
{}

Decoder::~Decoder() = default;

void Decoder::SetFrameDimensions(s32 width, s32 height) {
    if (width <= 0 || height <= 0) {
        frame_dimensions.reset();
        return;
    }
    frame_dimensions = FFmpeg::FrameDimensions{width, height};
}

void Decoder::Decode() {
    if (!initialized) {
        return;
    }

    const auto packet_data = ComposeFrame();
    FFmpeg::FrameOffsets offsets{};
    offsets.hidden = vp9_hidden_frame;
    offsets.interlaced = IsInterlaced();
    if (offsets.interlaced) {
        std::tie(offsets.luma, offsets.luma_bottom, std::ignore, std::ignore) =
            GetInterlacedOffsets();
    } else {
        std::tie(offsets.luma, std::ignore) = GetProgressiveOffsets();
    }

    // Send assembled bitstream to decoder.
    if (!decode_api.SendPacket(packet_data, offsets, GetFrameDimensions())) {
        return;
    }

    auto push = [&](u64 luma, std::shared_ptr<FFmpeg::Frame> frame) {
        if (UsingDecodeOrder()) {
            host1x.frame_queue.PushDecodeOrder(id, luma, std::move(frame));
        } else {
            host1x.frame_queue.PushPresentOrder(id, luma, std::move(frame));
        }
    };

    while (auto result = decode_api.ReceiveFrame()) {
        auto& [frame, frame_offsets] = *result;
        if (!frame) {
            continue;
        }
        if (frame_offsets.interlaced) {
            auto frame_copy = frame;
            push(frame_offsets.luma, std::move(frame));
            push(frame_offsets.luma_bottom, std::move(frame_copy));
        } else {
            push(frame_offsets.luma, std::move(frame));
        }
    }
}

} // namespace Tegra
