// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <vector>

#include "video_core/command_classes/codecs/vp8.h"

namespace Tegra::Decoder {
VP8::VP8(GPU& gpu_) : gpu(gpu_) {}

VP8::~VP8() = default;

const std::vector<u8>& VP8::ComposeFrameHeader(const NvdecCommon::NvdecRegisters& state,
                                               bool is_first_frame) {
    return {};
}

} // namespace Tegra::Decoder
