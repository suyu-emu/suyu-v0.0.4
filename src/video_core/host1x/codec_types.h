// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <vector>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"

namespace Tegra {

namespace Decoders {

struct Offset {
    constexpr u32 Address() const noexcept {
        return offset << 8;
    }

private:
    u32 offset;
};
static_assert(std::is_trivial_v<Offset>, "Offset must be trivial");
static_assert(sizeof(Offset) == 0x4, "Offset has the wrong size!");

struct H264ParameterSet {
    s32 log2_max_pic_order_cnt_lsb_minus4; ///< 0x00
    s32 delta_pic_order_always_zero_flag;  ///< 0x04
    s32 frame_mbs_only_flag;               ///< 0x08
    u32 pic_width_in_mbs;                  ///< 0x0C
    u32 frame_height_in_mbs;               ///< 0x10
    union {                                ///< 0x14
        BitField<0, 2, u32> tile_format;
        BitField<2, 3, u32> gob_height;
        BitField<5, 27, u32> reserved_surface_format;
    };
    u32 entropy_coding_mode_flag;               ///< 0x18
    s32 pic_order_present_flag;                 ///< 0x1C
    s32 num_refidx_l0_default_active;           ///< 0x20
    s32 num_refidx_l1_default_active;           ///< 0x24
    s32 deblocking_filter_control_present_flag; ///< 0x28
    s32 redundant_pic_cnt_present_flag;         ///< 0x2C
    u32 transform_8x8_mode_flag;                ///< 0x30
    u32 pitch_luma;                             ///< 0x34
    u32 pitch_chroma;                           ///< 0x38
    Offset luma_top_offset;                     ///< 0x3C
    Offset luma_bot_offset;                     ///< 0x40
    Offset luma_frame_offset;                   ///< 0x44
    Offset chroma_top_offset;                   ///< 0x48
    Offset chroma_bot_offset;                   ///< 0x4C
    Offset chroma_frame_offset;                 ///< 0x50
    u32 hist_buffer_size;                       ///< 0x54
    union {                                     ///< 0x58
        union {
            BitField<0, 1, u64> mbaff_frame;
            BitField<1, 1, u64> direct_8x8_inference;
            BitField<2, 1, u64> weighted_pred;
            BitField<3, 1, u64> constrained_intra_pred;
            BitField<4, 1, u64> ref_pic;
            BitField<5, 1, u64> field_pic;
            BitField<6, 1, u64> bottom_field;
            BitField<7, 1, u64> second_field;
        } flags;
        BitField<8, 4, u64> log2_max_frame_num_minus4;
        BitField<12, 2, u64> chroma_format_idc;
        BitField<14, 2, u64> pic_order_cnt_type;
        BitField<16, 6, s64> pic_init_qp_minus26;
        BitField<22, 5, s64> chroma_qp_index_offset;
        BitField<27, 5, s64> second_chroma_qp_index_offset;
        BitField<32, 2, u64> weighted_bipred_idc;
        BitField<34, 7, u64> curr_pic_idx;
        BitField<41, 5, u64> curr_col_idx;
        BitField<46, 16, u64> frame_number;
        BitField<62, 1, u64> frame_surfaces;
        BitField<63, 1, u64> output_memory_layout;
    };
};
static_assert(sizeof(H264ParameterSet) == 0x60, "H264ParameterSet is an invalid size");

#define ASSERT_POSITION(field_name, position)                                                      \
    static_assert(offsetof(H264ParameterSet, field_name) == position,                              \
                  "Field " #field_name " has invalid position")

ASSERT_POSITION(log2_max_pic_order_cnt_lsb_minus4, 0x00);
ASSERT_POSITION(delta_pic_order_always_zero_flag, 0x04);
ASSERT_POSITION(frame_mbs_only_flag, 0x08);
ASSERT_POSITION(pic_width_in_mbs, 0x0C);
ASSERT_POSITION(frame_height_in_mbs, 0x10);
ASSERT_POSITION(tile_format, 0x14);
ASSERT_POSITION(entropy_coding_mode_flag, 0x18);
ASSERT_POSITION(pic_order_present_flag, 0x1C);
ASSERT_POSITION(num_refidx_l0_default_active, 0x20);
ASSERT_POSITION(num_refidx_l1_default_active, 0x24);
ASSERT_POSITION(deblocking_filter_control_present_flag, 0x28);
ASSERT_POSITION(redundant_pic_cnt_present_flag, 0x2C);
ASSERT_POSITION(transform_8x8_mode_flag, 0x30);
ASSERT_POSITION(pitch_luma, 0x34);
ASSERT_POSITION(pitch_chroma, 0x38);
ASSERT_POSITION(luma_top_offset, 0x3C);
ASSERT_POSITION(luma_bot_offset, 0x40);
ASSERT_POSITION(luma_frame_offset, 0x44);
ASSERT_POSITION(chroma_top_offset, 0x48);
ASSERT_POSITION(chroma_bot_offset, 0x4C);
ASSERT_POSITION(chroma_frame_offset, 0x50);
ASSERT_POSITION(hist_buffer_size, 0x54);
ASSERT_POSITION(flags, 0x58);
#undef ASSERT_POSITION

struct DpbEntry {
    union {
        BitField<0, 7, u32> index;
        BitField<7, 5, u32> col_idx;
        BitField<12, 2, u32> state;
        BitField<14, 1, u32> is_long_term;
        BitField<15, 1, u32> non_existing;
        BitField<16, 1, u32> is_field;
        BitField<17, 4, u32> top_field_marking;
        BitField<21, 4, u32> bottom_field_marking;
        BitField<25, 1, u32> output_memory_layout;
        BitField<26, 6, u32> reserved;
    } flags;
    std::array<u32, 2> field_order_cnt;
    u32 frame_idx;
};
static_assert(sizeof(DpbEntry) == 0x10, "DpbEntry has the wrong size!");

struct DisplayParam {
    union {
        BitField<0, 1, u32> enable_tf_output;
        BitField<1, 1, u32> vc1_map_y_flag;
        BitField<2, 3, u32> map_y_value;
        BitField<5, 1, u32> vc1_map_uv_flag;
        BitField<6, 3, u32> map_uv_value;
        BitField<9, 8, u32> out_stride;
        BitField<17, 3, u32> tiling_format;
        BitField<20, 1, u32> output_structure; // 0=frame, 1=field
        BitField<21, 11, u32> reserved0;
    };
    std::array<s32, 2> output_top;
    std::array<s32, 2> output_bottom;
    union {
        BitField<0, 1, u32> enable_histogram;
        BitField<1, 12, u32> histogram_start_x;
        BitField<13, 12, u32> histogram_start_y;
        BitField<25, 7, u32> reserved1;
    };
    union {
        BitField<0, 12, u32> histogram_end_x;
        BitField<12, 12, u32> histogram_end_y;
        BitField<24, 8, u32> reserved2;
    };
};
static_assert(sizeof(DisplayParam) == 0x1C, "DisplayParam has the wrong size!");

struct H264DecoderContext {
    INSERT_PADDING_WORDS_NOINIT(13);                        ///< 0x0000
    std::array<u8, 16> eos;                                 ///< 0x0034
    u8 explicit_eos_present_flag;                           ///< 0x0044
    u8 hint_dump_en;                                        ///< 0x0045
    INSERT_PADDING_BYTES_NOINIT(2);                         ///< 0x0046
    u32 stream_len;                                         ///< 0x0048
    u32 slice_count;                                        ///< 0x004C
    u32 mbhist_buffer_size;                                 ///< 0x0050
    u32 gptimer_timeout_value;                              ///< 0x0054
    H264ParameterSet h264_parameter_set;                    ///< 0x0058
    std::array<s32, 2> curr_field_order_cnt;                ///< 0x00B8
    std::array<DpbEntry, 16> dpb;                           ///< 0x00C0
    std::array<u8, 0x60> weight_scale_4x4;                  ///< 0x01C0
    std::array<u8, 0x80> weight_scale_8x8;                  ///< 0x0220
    std::array<u8, 2> num_inter_view_refs_lX;               ///< 0x02A0
    std::array<u8, 14> reserved2;                           ///< 0x02A2
    std::array<std::array<s8, 16>, 2> inter_view_refidx_lX; ///< 0x02B0
    union {                                                 ///< 0x02D0
        BitField<0, 1, u32> lossless_ipred8x8_filter_enable;
        BitField<1, 1, u32> qpprime_y_zero_transform_bypass_flag;
        BitField<2, 30, u32> reserved3;
    };
    DisplayParam display_param;   ///< 0x02D4
    std::array<u32, 3> reserved4; ///< 0x02F0
};
static_assert(sizeof(H264DecoderContext) == 0x2FC, "H264DecoderContext is an invalid size");

#define ASSERT_POSITION(field_name, position)                                                      \
    static_assert(offsetof(H264DecoderContext, field_name) == position,                            \
                  "Field " #field_name " has invalid position")

ASSERT_POSITION(stream_len, 0x48);
ASSERT_POSITION(h264_parameter_set, 0x58);
ASSERT_POSITION(dpb, 0xC0);
ASSERT_POSITION(weight_scale_4x4, 0x1C0);
#undef ASSERT_POSITION

enum class Vp9SurfaceIndex : u32 {
    Last = 0,
    Golden = 1,
    AltRef = 2,
    Current = 3,
};

struct Vp9FrameDimensions {
    s16 width;
    s16 height;
    s16 luma_pitch;
    s16 chroma_pitch;
};
static_assert(sizeof(Vp9FrameDimensions) == 0x8, "Vp9 Vp9FrameDimensions is an invalid size");

enum class FrameFlags : u32 {
    IsKeyFrame = 1 << 0,
    LastFrameIsKeyFrame = 1 << 1,
    FrameSizeChanged = 1 << 2,
    ErrorResilientMode = 1 << 3,
    LastShowFrame = 1 << 4,
    IntraOnly = 1 << 5,
};
DECLARE_ENUM_FLAG_OPERATORS(FrameFlags)

enum class TxSize {
    Tx4x4 = 0,   // 4x4 transform
    Tx8x8 = 1,   // 8x8 transform
    Tx16x16 = 2, // 16x16 transform
    Tx32x32 = 3, // 32x32 transform
    TxSizes = 4
};

enum class TxMode {
    Only4X4 = 0,      // Only 4x4 transform used
    Allow8X8 = 1,     // Allow block transform size up to 8x8
    Allow16X16 = 2,   // Allow block transform size up to 16x16
    Allow32X32 = 3,   // Allow block transform size up to 32x32
    TxModeSelect = 4, // Transform specified for each block
    TxModes = 5
};

struct Segmentation {
    constexpr bool operator==(const Segmentation& rhs) const = default;

    u8 enabled;
    u8 update_map;
    u8 temporal_update;
    u8 abs_delta;
    std::array<std::array<u8, 4>, 8> feature_enabled;
    std::array<std::array<s16, 4>, 8> feature_data;
};
static_assert(sizeof(Segmentation) == 0x64, "Segmentation is an invalid size");

struct LoopFilter {
    u8 mode_ref_delta_enabled;
    std::array<s8, 4> ref_deltas;
    std::array<s8, 2> mode_deltas;
};
static_assert(sizeof(LoopFilter) == 0x7, "LoopFilter is an invalid size");

struct Vp9EntropyProbs {
    std::array<u8, 36> y_mode_prob;           ///< 0x0000
    std::array<u8, 64> partition_prob;        ///< 0x0024
    std::array<u8, 1728> coef_probs;          ///< 0x0064
    std::array<u8, 8> switchable_interp_prob; ///< 0x0724
    std::array<u8, 28> inter_mode_prob;       ///< 0x072C
    std::array<u8, 4> intra_inter_prob;       ///< 0x0748
    std::array<u8, 5> comp_inter_prob;        ///< 0x074C
    std::array<u8, 10> single_ref_prob;       ///< 0x0751
    std::array<u8, 5> comp_ref_prob;          ///< 0x075B
    std::array<u8, 6> tx_32x32_prob;          ///< 0x0760
    std::array<u8, 4> tx_16x16_prob;          ///< 0x0766
    std::array<u8, 2> tx_8x8_prob;            ///< 0x076A
    std::array<u8, 3> skip_probs;             ///< 0x076C
    std::array<u8, 3> joints;                 ///< 0x076F
    std::array<u8, 2> sign;                   ///< 0x0772
    std::array<u8, 20> classes;               ///< 0x0774
    std::array<u8, 2> class_0;                ///< 0x0788
    std::array<u8, 20> prob_bits;             ///< 0x078A
    std::array<u8, 12> class_0_fr;            ///< 0x079E
    std::array<u8, 6> fr;                     ///< 0x07AA
    std::array<u8, 2> class_0_hp;             ///< 0x07B0
    std::array<u8, 2> high_precision;         ///< 0x07B2
};
static_assert(sizeof(Vp9EntropyProbs) == 0x7B4, "Vp9EntropyProbs is an invalid size");

struct Vp9PictureInfo {
    u32 bitstream_size;
    std::array<u64, 4> frame_offsets;
    std::array<s8, 4> ref_frame_sign_bias;
    s32 base_q_index;
    s32 y_dc_delta_q;
    s32 uv_dc_delta_q;
    s32 uv_ac_delta_q;
    s32 transform_mode;
    s32 interp_filter;
    s32 reference_mode;
    s32 log2_tile_cols;
    s32 log2_tile_rows;
    std::array<s8, 4> ref_deltas;
    std::array<s8, 2> mode_deltas;
    Vp9EntropyProbs entropy;
    Vp9FrameDimensions frame_size;
    u8 first_level;
    u8 sharpness_level;
    bool is_key_frame;
    bool intra_only;
    bool last_frame_was_key;
    bool error_resilient_mode;
    bool last_frame_shown;
    bool show_frame;
    bool lossless;
    bool allow_high_precision_mv;
    bool segment_enabled;
    bool mode_ref_delta_enabled;
};

struct Vp9FrameContainer {
    Vp9PictureInfo info{};
    std::vector<u8> bit_stream;
};

struct PictureInfo {
    INSERT_PADDING_WORDS_NOINIT(12);       ///< 0x00
    u32 bitstream_size;                    ///< 0x30
    INSERT_PADDING_WORDS_NOINIT(5);        ///< 0x34
    Vp9FrameDimensions last_frame_size;    ///< 0x48
    Vp9FrameDimensions golden_frame_size;  ///< 0x50
    Vp9FrameDimensions alt_frame_size;     ///< 0x58
    Vp9FrameDimensions current_frame_size; ///< 0x60
    FrameFlags vp9_flags;                  ///< 0x68
    std::array<s8, 4> ref_frame_sign_bias; ///< 0x6C
    u8 first_level;                        ///< 0x70
    u8 sharpness_level;                    ///< 0x71
    u8 base_q_index;                       ///< 0x72
    u8 y_dc_delta_q;                       ///< 0x73
    u8 uv_ac_delta_q;                      ///< 0x74
    u8 uv_dc_delta_q;                      ///< 0x75
    u8 lossless;                           ///< 0x76
    u8 tx_mode;                            ///< 0x77
    u8 allow_high_precision_mv;            ///< 0x78
    u8 interp_filter;                      ///< 0x79
    u8 reference_mode;                     ///< 0x7A
    INSERT_PADDING_BYTES_NOINIT(3);        ///< 0x7B
    u8 log2_tile_cols;                     ///< 0x7E
    u8 log2_tile_rows;                     ///< 0x7F
    Segmentation segmentation;             ///< 0x80
    LoopFilter loop_filter;                ///< 0xE4
    INSERT_PADDING_BYTES_NOINIT(21);       ///< 0xEB

    [[nodiscard]] Vp9PictureInfo Convert() const {
        return {
            .bitstream_size = bitstream_size,
            .frame_offsets{},
            .ref_frame_sign_bias = ref_frame_sign_bias,
            .base_q_index = base_q_index,
            .y_dc_delta_q = y_dc_delta_q,
            .uv_dc_delta_q = uv_dc_delta_q,
            .uv_ac_delta_q = uv_ac_delta_q,
            .transform_mode = tx_mode,
            .interp_filter = interp_filter,
            .reference_mode = reference_mode,
            .log2_tile_cols = log2_tile_cols,
            .log2_tile_rows = log2_tile_rows,
            .ref_deltas = loop_filter.ref_deltas,
            .mode_deltas = loop_filter.mode_deltas,
            .entropy{},
            .frame_size = current_frame_size,
            .first_level = first_level,
            .sharpness_level = sharpness_level,
            .is_key_frame = True(vp9_flags & FrameFlags::IsKeyFrame),
            .intra_only = True(vp9_flags & FrameFlags::IntraOnly),
            .last_frame_was_key = True(vp9_flags & FrameFlags::LastFrameIsKeyFrame),
            .error_resilient_mode = True(vp9_flags & FrameFlags::ErrorResilientMode),
            .last_frame_shown = True(vp9_flags & FrameFlags::LastShowFrame),
            .show_frame = true,
            .lossless = lossless != 0,
            .allow_high_precision_mv = allow_high_precision_mv != 0,
            .segment_enabled = segmentation.enabled != 0,
            .mode_ref_delta_enabled = loop_filter.mode_ref_delta_enabled != 0,
        };
    }
};
static_assert(sizeof(PictureInfo) == 0x100, "PictureInfo is an invalid size");

struct EntropyProbs {
    std::array<u8, 10 * 10 * 8> kf_bmode_prob;         ///< 0x0000
    std::array<u8, 10 * 10 * 1> kf_bmode_probB;        ///< 0x0320
    std::array<u8, 3> ref_pred_probs;                  ///< 0x0384
    std::array<u8, 7> mb_segment_tree_probs;           ///< 0x0387
    std::array<u8, 3> segment_pred_probs;              ///< 0x038E
    std::array<u8, 4> ref_scores;                      ///< 0x0391
    std::array<u8, 2> prob_comppred;                   ///< 0x0395
    INSERT_PADDING_BYTES_NOINIT(9);                    ///< 0x0397
    std::array<u8, 10 * 8> kf_uv_mode_prob;            ///< 0x03A0
    std::array<u8, 10 * 1> kf_uv_mode_probB;           ///< 0x03F0
    INSERT_PADDING_BYTES_NOINIT(6);                    ///< 0x03FA
    std::array<u8, 28> inter_mode_prob;                ///< 0x0400
    std::array<u8, 4> intra_inter_prob;                ///< 0x041C
    INSERT_PADDING_BYTES_NOINIT(80);                   ///< 0x0420
    std::array<u8, 2> tx_8x8_prob;                     ///< 0x0470
    std::array<u8, 4> tx_16x16_prob;                   ///< 0x0472
    std::array<u8, 6> tx_32x32_prob;                   ///< 0x0476
    std::array<u8, 4> y_mode_prob_e8;                  ///< 0x047C
    std::array<std::array<u8, 8>, 4> y_mode_prob_e0e7; ///< 0x0480
    INSERT_PADDING_BYTES_NOINIT(64);                   ///< 0x04A0
    std::array<u8, 64> partition_prob;                 ///< 0x04E0
    INSERT_PADDING_BYTES_NOINIT(10);                   ///< 0x0520
    std::array<u8, 8> switchable_interp_prob;          ///< 0x052A
    std::array<u8, 5> comp_inter_prob;                 ///< 0x0532
    std::array<u8, 3> skip_probs;                      ///< 0x0537
    INSERT_PADDING_BYTES_NOINIT(1);                    ///< 0x053A
    std::array<u8, 3> joints;                          ///< 0x053B
    std::array<u8, 2> sign;                            ///< 0x053E
    std::array<u8, 2> class_0;                         ///< 0x0540
    std::array<u8, 6> fr;                              ///< 0x0542
    std::array<u8, 2> class_0_hp;                      ///< 0x0548
    std::array<u8, 2> high_precision;                  ///< 0x054A
    std::array<u8, 20> classes;                        ///< 0x054C
    std::array<u8, 12> class_0_fr;                     ///< 0x0560
    std::array<u8, 20> pred_bits;                      ///< 0x056C
    std::array<u8, 10> single_ref_prob;                ///< 0x0580
    std::array<u8, 5> comp_ref_prob;                   ///< 0x058A
    INSERT_PADDING_BYTES_NOINIT(17);                   ///< 0x058F
    std::array<u8, 2304> coef_probs;                   ///< 0x05A0

    void Convert(Vp9EntropyProbs& fc) {
        fc.inter_mode_prob = inter_mode_prob;
        fc.intra_inter_prob = intra_inter_prob;
        fc.tx_8x8_prob = tx_8x8_prob;
        fc.tx_16x16_prob = tx_16x16_prob;
        fc.tx_32x32_prob = tx_32x32_prob;

        for (std::size_t i = 0; i < 4; i++) {
            for (std::size_t j = 0; j < 9; j++) {
                fc.y_mode_prob[j + 9 * i] = j < 8 ? y_mode_prob_e0e7[i][j] : y_mode_prob_e8[i];
            }
        }

        fc.partition_prob = partition_prob;
        fc.switchable_interp_prob = switchable_interp_prob;
        fc.comp_inter_prob = comp_inter_prob;
        fc.skip_probs = skip_probs;
        fc.joints = joints;
        fc.sign = sign;
        fc.class_0 = class_0;
        fc.fr = fr;
        fc.class_0_hp = class_0_hp;
        fc.high_precision = high_precision;
        fc.classes = classes;
        fc.class_0_fr = class_0_fr;
        fc.prob_bits = pred_bits;
        fc.single_ref_prob = single_ref_prob;
        fc.comp_ref_prob = comp_ref_prob;

        // Skip the 4th element as it goes unused
        for (std::size_t i = 0; i < coef_probs.size(); i += 4) {
            const std::size_t j = i - i / 4;
            fc.coef_probs[j] = coef_probs[i];
            fc.coef_probs[j + 1] = coef_probs[i + 1];
            fc.coef_probs[j + 2] = coef_probs[i + 2];
        }
    }
};
static_assert(sizeof(EntropyProbs) == 0xEA0, "EntropyProbs is an invalid size");

enum class Ref { Last, Golden, AltRef };

struct RefPoolElement {
    s64 frame{};
    Ref ref{};
    bool refresh{};
};

#define ASSERT_POSITION(field_name, position) static_assert(offsetof(Vp9EntropyProbs, field_name) == position)
ASSERT_POSITION(partition_prob, 0x0024);
ASSERT_POSITION(switchable_interp_prob, 0x0724);
ASSERT_POSITION(sign, 0x0772);
ASSERT_POSITION(class_0_fr, 0x079E);
ASSERT_POSITION(high_precision, 0x07B2);
#undef ASSERT_POSITION

#define ASSERT_POSITION(field_name, position) static_assert(offsetof(PictureInfo, field_name) == position)
ASSERT_POSITION(bitstream_size, 0x30);
ASSERT_POSITION(last_frame_size, 0x48);
ASSERT_POSITION(first_level, 0x70);
ASSERT_POSITION(segmentation, 0x80);
ASSERT_POSITION(loop_filter, 0xE4);
#undef ASSERT_POSITION

#define ASSERT_POSITION(field_name, position) static_assert(offsetof(EntropyProbs, field_name) == position)

ASSERT_POSITION(inter_mode_prob, 0x400);
ASSERT_POSITION(tx_8x8_prob, 0x470);
ASSERT_POSITION(partition_prob, 0x4E0);
ASSERT_POSITION(class_0, 0x540);
ASSERT_POSITION(class_0_fr, 0x560);
ASSERT_POSITION(coef_probs, 0x5A0);
#undef ASSERT_POSITION

struct VP8PictureInfo {
    INSERT_PADDING_WORDS_NOINIT(14);
    u16 frame_width;  // actual frame width
    u16 frame_height; // actual frame height
    u8 key_frame;
    u8 version;
    union {
        u8 raw;
        BitField<0, 2, u8> tile_format;
        BitField<2, 3, u8> gob_height;
        BitField<5, 3, u8> reserved_surface_format;
    };
    u8 error_conceal_on;  // 1: error conceal on; 0: off
    u32 first_part_size;  // the size of first partition(frame header and mb header partition)
    u32 hist_buffer_size; // in units of 256
    u32 vld_buffer_size;  // in units of 1
    // Current frame buffers
    std::array<u32, 2> frame_stride; // [y_c]
    u32 luma_top_offset;             // offset of luma top field in units of 256
    u32 luma_bot_offset;             // offset of luma bottom field in units of 256
    u32 luma_frame_offset;           // offset of luma frame in units of 256
    u32 chroma_top_offset;           // offset of chroma top field in units of 256
    u32 chroma_bot_offset;           // offset of chroma bottom field in units of 256
    u32 chroma_frame_offset;         // offset of chroma frame in units of 256

    INSERT_PADDING_BYTES_NOINIT(0x1c); // NvdecDisplayParams

    // Decode picture buffer related
    s8 current_output_memory_layout;
    // output NV12/NV24 setting. index 0: golden; 1: altref; 2: last
    std::array<s8, 3> output_memory_layout;

    u8 segmentation_feature_data_update;
    INSERT_PADDING_BYTES_NOINIT(3);

    // ucode return result
    u32 result_value;
    std::array<u32, 8> partition_offset;
    INSERT_PADDING_WORDS_NOINIT(3);
};
static_assert(sizeof(VP8PictureInfo) == 0xc0, "PictureInfo is an invalid size");

}; // namespace Decoders
}; // namespace Tegra
