// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <span>
#include <vector>

#include "common/common_types.h"
#include "common/scratch_buffer.h"
#include "video_core/host1x/codecs/decoder.h"
#include "video_core/host1x/codec_types.h"
#include "video_core/host1x/nvdec_common.h"

namespace Tegra {

namespace Host1x {
class Host1x;
} // namespace Host1x

namespace Decoders {

class H264BitWriter {
public:
    H264BitWriter();
    ~H264BitWriter();

    /// The following Write methods are based on clause 9.1 in the H.264 specification.
    /// WriteSe and WriteUe write in the Exp-Golomb-coded syntax
    void WriteU(s32 value, s32 value_sz);
    void WriteSe(s32 value);
    void WriteUe(u32 value);

    /// Finalize the bitstream
    void End();

    /// append a bit to the stream, equivalent value to the state parameter
    void WriteBit(bool state);

    /// Based on section 7.3.2.1.1.1 and Table 7-4 in the H.264 specification
    /// Writes the scaling matrices of the sream
    void WriteScalingList(std::span<const u8> list, s32 start, s32 count);

    /// Return the bitstream as a vector.
    [[nodiscard]] std::vector<u8>& GetByteArray();
    [[nodiscard]] const std::vector<u8>& GetByteArray() const;

private:
    void WriteBits(s32 value, s32 bit_count);
    void WriteExpGolombCodedInt(s32 value);
    void WriteExpGolombCodedUInt(u32 value);
    [[nodiscard]] s32 GetFreeBufferBits();
    void Flush();

    s32 buffer_size{8};

    s32 buffer{};
    s32 buffer_pos{};
    std::vector<u8> byte_array;
};

class H264 final : public Decoder {
public:
    explicit H264(Host1x::Host1x& host1x, const Host1x::NvdecCommon::NvdecRegisters& regs, s32 id);
    ~H264() override;

    H264(const H264&) = delete;
    H264& operator=(const H264&) = delete;

    H264(H264&&) = delete;
    H264& operator=(H264&&) = delete;

    /// Compose the H264 frame for FFmpeg decoding
    [[nodiscard]] std::span<const u8> ComposeFrame() override;

    std::tuple<u64, u64> GetProgressiveOffsets() override;
    std::tuple<u64, u64, u64, u64> GetInterlacedOffsets() override;
    bool IsInterlaced() override;

    std::string_view GetCurrentCodecName() const override {
        return "H264";
    }

private:
    H264DecoderContext current_context{};
    std::array<u8, 64> scan_scratch;
    Common::ScratchBuffer<u8> frame_scratch;
    bool is_first_frame{true};
};

} // namespace Decoders
} // namespace Tegra
