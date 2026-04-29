// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <variant>
#include "common/assert.h"

#include "common/polyfill_thread.h"
#include "common/settings.h"
#include "video_core/host1x/codecs/h264.h"
#include "video_core/host1x/codecs/vp8.h"
#include "video_core/host1x/codecs/vp9.h"
#include "video_core/host1x/host1x.h"
#include "video_core/host1x/nvdec.h"

namespace Tegra::Host1x {

#define NVDEC_REG_INDEX(field_name)                                                                \
    (offsetof(NvdecCommon::NvdecRegisters, field_name) / sizeof(u64))

Nvdec::Nvdec(Host1x& host1x_, s32 id_, u32 syncpt)
    : CDmaPusher{host1x_, id_}
    , id{id_}
    , syncpoint{syncpt}
{
    LOG_INFO(HW_GPU, "Created nvdec {}", id);
    host1x.frame_queue.Open(id);
}

Nvdec::~Nvdec() {
    LOG_INFO(HW_GPU, "Destroying nvdec {}", id);
}

void Nvdec::ProcessMethod(u32 method, u32 argument) {
    regs.reg_array[method] = argument;

    switch (method) {
    case NVDEC_REG_INDEX(set_codec_id):
        CreateDecoder(static_cast<NvdecCommon::VideoCodec>(argument));
        break;
    case NVDEC_REG_INDEX(execute): {
        Execute();
    } break;
    }
}

void Nvdec::CreateDecoder(NvdecCommon::VideoCodec codec) {
    if (std::holds_alternative<std::monostate>(decoder)) {
        switch (codec) {
        case NvdecCommon::VideoCodec::H264:
            decoder.emplace<Decoders::H264>(host1x, regs, id);
            break;
        case NvdecCommon::VideoCodec::VP8:
            decoder.emplace<Decoders::VP8>(host1x, regs, id);
            break;
        case NvdecCommon::VideoCodec::VP9:
            decoder.emplace<Decoders::VP9>(host1x, regs, id);
            break;
        default:
            break;
        }
        LOG_INFO(HW_GPU, "Created decoder {} for id {}", codec, id);
    }
}

void Nvdec::Execute() {
    if (Settings::values.nvdec_emulation.GetValue() == Settings::NvdecEmulation::Off) [[unlikely]] {
        // Signalling syncpts too fast can cause games to get stuck as they don't expect a <1ms
        // execution time. Sleep for half of a 60 fps frame just in case.
        std::this_thread::sleep_for(std::chrono::milliseconds(8));
        return;
    }
    if (auto* h264 = std::get_if<Decoders::H264>(&decoder)) {
        h264->Decode();
    } else if (auto* vp8 = std::get_if<Decoders::VP8>(&decoder)) {
        vp8->Decode();
    } else if (auto* vp9 = std::get_if<Decoders::VP9>(&decoder)) {
        vp9->Decode();
    } else {
        LOG_ERROR(HW_GPU, "Unrecognized codec executed?");
    }
}

} // namespace Tegra::Host1x
