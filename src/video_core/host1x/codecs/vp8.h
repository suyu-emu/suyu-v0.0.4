// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <span>

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/scratch_buffer.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/nvdec_common.h"
#include "video_core/host1x/codec_types.h"

namespace Tegra {

namespace Host1x {
class Host1x;
} // namespace Host1x

namespace Decoders {
enum class Vp8SurfaceIndex : u32 {
    Last = 0,
    Golden = 1,
    AltRef = 2,
    Current = 3,
};

class VP8 final : public Decoder {
public:
    explicit VP8(Host1x::Host1x& host1x, const Host1x::NvdecCommon::NvdecRegisters& regs, s32 id);
    ~VP8() override;

    VP8(const VP8&) = delete;
    VP8& operator=(const VP8&) = delete;

    VP8(VP8&&) = delete;
    VP8& operator=(VP8&&) = delete;

    [[nodiscard]] std::span<const u8> ComposeFrame() override;

    std::tuple<u64, u64> GetProgressiveOffsets() override;
    std::tuple<u64, u64, u64, u64> GetInterlacedOffsets() override;

    bool IsInterlaced() override {
        return false;
    }

    std::string_view GetCurrentCodecName() const override {
        return "VP8";
    }

private:
    VP8PictureInfo current_context{};
    Common::ScratchBuffer<u8> frame_scratch;
};

} // namespace Decoders
} // namespace Tegra
