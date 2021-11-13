// Copyright 2021 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"
#include "video_core/command_classes/nvdec_common.h"

namespace Tegra {
class GPU;
namespace Decoder {

class VP8 {
public:
    explicit VP8(GPU& gpu);
    ~VP8();

    /// Compose the VP8 header of the frame for FFmpeg decoding
    [[nodiscard]] const std::vector<u8>& ComposeFrameHeader(
        const NvdecCommon::NvdecRegisters& state, bool is_first_frame = false);

private:
    std::vector<u8> frame;
    GPU& gpu;
};

} // namespace Decoder
} // namespace Tegra
