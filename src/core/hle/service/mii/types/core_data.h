// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"

namespace Service::Mii {

struct StoreDataBitFields {
    union {
        u32 word_0{};

        BitField<0, 8, u32> hair_type;
        BitField<8, 7, u32> height;
        BitField<15, 1, u32> mole_type;
        BitField<16, 7, u32> build;
        BitField<23, 1, HairFlip> hair_flip;
        BitField<24, 7, u32> hair_color;
        BitField<31, 1, u32> type;
    };

    union {
        u32 word_1{};

        BitField<0, 7, u32> eye_color;
        BitField<7, 1, Gender> gender;
        BitField<8, 7, u32> eyebrow_color;
        BitField<16, 7, u32> mouth_color;
        BitField<24, 7, u32> beard_color;
    };

    union {
        u32 word_2{};

        BitField<0, 7, u32> glasses_color;
        BitField<8, 6, u32> eye_type;
        BitField<14, 2, u32> region_move;
        BitField<16, 6, u32> mouth_type;
        BitField<22, 2, FontRegion> font_region;
        BitField<24, 5, u32> eye_y;
        BitField<29, 3, u32> glasses_scale;
    };

    union {
        u32 word_3{};

        BitField<0, 5, u32> eyebrow_type;
        BitField<5, 3, MustacheType> mustache_type;
        BitField<8, 5, u32> nose_type;
        BitField<13, 3, BeardType> beard_type;
        BitField<16, 5, u32> nose_y;
        BitField<21, 3, u32> mouth_aspect;
        BitField<24, 5, u32> mouth_y;
        BitField<29, 3, u32> eyebrow_aspect;
    };

    union {
        u32 word_4{};

        BitField<0, 5, u32> mustache_y;
        BitField<5, 3, u32> eye_rotate;
        BitField<8, 5, u32> glasses_y;
        BitField<13, 3, u32> eye_aspect;
        BitField<16, 5, u32> mole_x;
        BitField<21, 3, u32> eye_scale;
        BitField<24, 5, u32> mole_y;
    };

    union {
        u32 word_5{};

        BitField<0, 5, u32> glasses_type;
        BitField<8, 4, u32> favorite_color;
        BitField<12, 4, u32> faceline_type;
        BitField<16, 4, u32> faceline_color;
        BitField<20, 4, u32> faceline_wrinkle;
        BitField<24, 4, u32> faceline_makeup;
        BitField<28, 4, u32> eye_x;
    };

    union {
        u32 word_6{};

        BitField<0, 4, u32> eyebrow_scale;
        BitField<4, 4, u32> eyebrow_rotate;
        BitField<8, 4, u32> eyebrow_x;
        BitField<12, 4, u32> eyebrow_y;
        BitField<16, 4, u32> nose_scale;
        BitField<20, 4, u32> mouth_scale;
        BitField<24, 4, u32> mustache_scale;
        BitField<28, 4, u32> mole_scale;
    };
};
static_assert(sizeof(StoreDataBitFields) == 0x1c, "StoreDataBitFields has incorrect size.");
static_assert(std::is_trivially_copyable_v<StoreDataBitFields>,
              "StoreDataBitFields is not trivially copyable.");

struct CoreData {
    StoreDataBitFields data{};
    Nickname name{};
};
static_assert(sizeof(CoreData) == 0x30, "CoreData has incorrect size.");

}; // namespace Service::Mii
