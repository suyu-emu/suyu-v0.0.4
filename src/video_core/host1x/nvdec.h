// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <memory>
#include <vector>
#include <variant>

#include "common/common_types.h"
#include "video_core/cdma_pusher.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/codecs/h264.h"
#include "video_core/host1x/codecs/vp8.h"
#include "video_core/host1x/codecs/vp9.h"

namespace Tegra {

namespace Host1x {
class Host1x;
class FrameQueue;

class Nvdec final : public CDmaPusher {
public:
    explicit Nvdec(Host1x& host1x, s32 id, u32 syncpt);
    ~Nvdec();

    /// Writes the method into the state, Invoke Execute() if encountered
    void ProcessMethod(u32 method, u32 arg) override;

    u32 GetSyncpoint() const {
        return syncpoint;
    }

private:
    /// Create the decoder when the codec id is set
    void CreateDecoder(NvdecCommon::VideoCodec codec);

    /// Invoke codec to decode a frame
    void Execute();

    NvdecCommon::NvdecRegisters regs{};
    std::variant<
        Decoders::H264,
        Decoders::VP8,
        Decoders::VP9,
        std::monostate
    > decoder = std::monostate{};
    s32 id;
    u32 syncpoint;
};

} // namespace Host1x

} // namespace Tegra
