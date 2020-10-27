// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <vector>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/stream.h"
#include "video_core/command_classes/codecs/vp9_types.h"
#include "video_core/command_classes/nvdec_common.h"

namespace Tegra {
class GPU;
enum class FrameType { KeyFrame = 0, InterFrame = 1 };
namespace Decoder {

/// The VpxRangeEncoder, and VpxBitStreamWriter classes are used to compose the
/// VP9 header bitstreams.

class VpxRangeEncoder {
public:
    VpxRangeEncoder();
    ~VpxRangeEncoder();

    /// Writes the rightmost value_size bits from value into the stream
    void Write(s32 value, s32 value_size);

    /// Writes a single bit with half probability
    void Write(bool bit);

    /// Writes a bit to the base_stream encoded with probability
    void Write(bool bit, s32 probability);

    /// Signal the end of the bitstream
    void End();

    std::vector<u8>& GetBuffer() {
        return base_stream.GetBuffer();
    }

    const std::vector<u8>& GetBuffer() const {
        return base_stream.GetBuffer();
    }

private:
    u8 PeekByte();
    Common::Stream base_stream{};
    u32 low_value{};
    u32 range{0xff};
    s32 count{-24};
    s32 half_probability{128};
    static constexpr std::array<s32, 256> norm_lut{
        0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
        3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
        2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    };
};

class VpxBitStreamWriter {
public:
    VpxBitStreamWriter();
    ~VpxBitStreamWriter();

    /// Write an unsigned integer value
    void WriteU(u32 value, u32 value_size);

    /// Write a signed integer value
    void WriteS(s32 value, u32 value_size);

    /// Based on 6.2.10 of VP9 Spec, writes a delta coded value
    void WriteDeltaQ(u32 value);

    /// Write a single bit.
    void WriteBit(bool state);

    /// Pushes current buffer into buffer_array, resets buffer
    void Flush();

    /// Returns byte_array
    std::vector<u8>& GetByteArray();

    /// Returns const byte_array
    const std::vector<u8>& GetByteArray() const;

private:
    /// Write bit_count bits from value into buffer
    void WriteBits(u32 value, u32 bit_count);

    /// Gets next available position in buffer, invokes Flush() if buffer is full
    s32 GetFreeBufferBits();

    s32 buffer_size{8};

    s32 buffer{};
    s32 buffer_pos{};
    std::vector<u8> byte_array;
};

class VP9 {
public:
    explicit VP9(GPU& gpu);
    ~VP9();

    /// Composes the VP9 frame from the GPU state information. Based on the official VP9 spec
    /// documentation
    std::vector<u8>& ComposeFrameHeader(NvdecCommon::NvdecRegisters& state);

    /// Returns true if the most recent frame was a hidden frame.
    bool WasFrameHidden() const {
        return hidden;
    }

private:
    /// Generates compressed header probability updates in the bitstream writer
    template <typename T, std::size_t N>
    void WriteProbabilityUpdate(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                const std::array<T, N>& old_prob);

    /// Generates compressed header probability updates in the bitstream writer
    /// If probs are not equal, WriteProbabilityDelta is invoked
    void WriteProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob);

    /// Generates compressed header probability deltas in the bitstream writer
    void WriteProbabilityDelta(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob);

    /// Adjusts old_prob depending on new_prob. Based on section 6.3.5 of VP9 Specification
    s32 RemapProbability(s32 new_prob, s32 old_prob);

    /// Recenters probability. Based on section 6.3.6 of VP9 Specification
    s32 RecenterNonNeg(s32 new_prob, s32 old_prob);

    /// Inverse of 6.3.4 Decode term subexp
    void EncodeTermSubExp(VpxRangeEncoder& writer, s32 value);

    /// Writes if the value is less than the test value
    bool WriteLessThan(VpxRangeEncoder& writer, s32 value, s32 test);

    /// Writes probability updates for the Coef probabilities
    void WriteCoefProbabilityUpdate(VpxRangeEncoder& writer, s32 tx_mode,
                                    const std::array<u8, 2304>& new_prob,
                                    const std::array<u8, 2304>& old_prob);

    /// Write probabilities for 4-byte aligned structures
    template <typename T, std::size_t N>
    void WriteProbabilityUpdateAligned4(VpxRangeEncoder& writer, const std::array<T, N>& new_prob,
                                        const std::array<T, N>& old_prob);

    /// Write motion vector probability updates. 6.3.17 in the spec
    void WriteMvProbabilityUpdate(VpxRangeEncoder& writer, u8 new_prob, u8 old_prob);

    /// 6.2.14 Tile size calculation
    s32 CalcMinLog2TileCols(s32 frame_width);
    s32 CalcMaxLog2TileCols(s32 frame_width);

    /// Returns VP9 information from NVDEC provided offset and size
    Vp9PictureInfo GetVp9PictureInfo(const NvdecCommon::NvdecRegisters& state);

    /// Read and convert NVDEC provided entropy probs to Vp9EntropyProbs struct
    void InsertEntropy(u64 offset, Vp9EntropyProbs& dst);

    /// Returns frame to be decoded after buffering
    Vp9FrameContainer GetCurrentFrame(const NvdecCommon::NvdecRegisters& state);

    /// Use NVDEC providied information to compose the headers for the current frame
    std::vector<u8> ComposeCompressedHeader();
    VpxBitStreamWriter ComposeUncompressedHeader();

    GPU& gpu;
    std::vector<u8> frame;

    std::array<s8, 4> loop_filter_ref_deltas{};
    std::array<s8, 2> loop_filter_mode_deltas{};

    bool hidden;
    s64 current_frame_number = -2; // since we buffer 2 frames
    s32 grace_period = 6;          // frame offsets need to stabilize
    std::array<FrameContexts, 4> frame_ctxs{};
    Vp9FrameContainer next_frame{};
    Vp9FrameContainer next_next_frame{};
    bool swap_next_golden{};

    Vp9PictureInfo current_frame_info{};
    Vp9EntropyProbs prev_frame_probs{};

    s32 diff_update_probability = 252;
    s32 frame_sync_code = 0x498342;
    static constexpr std::array<s32, 254> map_lut = {
        20,  21,  22,  23,  24,  25,  0,   26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
        36,  37,  1,   38,  39,  40,  41,  42,  43,  44,  45,  46,  47,  48,  49,  2,   50,
        51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  3,   62,  63,  64,  65,  66,
        67,  68,  69,  70,  71,  72,  73,  4,   74,  75,  76,  77,  78,  79,  80,  81,  82,
        83,  84,  85,  5,   86,  87,  88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  6,
        98,  99,  100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 7,   110, 111, 112, 113,
        114, 115, 116, 117, 118, 119, 120, 121, 8,   122, 123, 124, 125, 126, 127, 128, 129,
        130, 131, 132, 133, 9,   134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145,
        10,  146, 147, 148, 149, 150, 151, 152, 153, 154, 155, 156, 157, 11,  158, 159, 160,
        161, 162, 163, 164, 165, 166, 167, 168, 169, 12,  170, 171, 172, 173, 174, 175, 176,
        177, 178, 179, 180, 181, 13,  182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192,
        193, 14,  194, 195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 15,  206, 207,
        208, 209, 210, 211, 212, 213, 214, 215, 216, 217, 16,  218, 219, 220, 221, 222, 223,
        224, 225, 226, 227, 228, 229, 17,  230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
        240, 241, 18,  242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 19,
    };
};

} // namespace Decoder
} // namespace Tegra
