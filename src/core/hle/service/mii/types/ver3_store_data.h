// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"
#include "core/hle/service/mii/types/core_data.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

// This is nn::mii::Ver3StoreData
// Based on citra HLE::Applets::MiiData and PretendoNetwork.
// https://github.com/citra-emu/citra/blob/master/src/core/hle/applets/mii_selector.h#L48
// https://github.com/PretendoNetwork/mii-js/blob/master/mii.js#L299

#pragma pack(push, 4)
struct Ver3StoreData {
    u8 version;
    union {
        u8 raw;

        BitField<0, 1, u8> allow_copying;
        BitField<1, 1, u8> profanity_flag;
        BitField<2, 2, u8> region_lock;
        BitField<4, 2, u8> character_set;
    } region_information;
    u16_be mii_id;
    u64_be system_id;
    u32_be specialness_and_creation_date;
    std::array<u8, 0x6> creator_mac;
    u16_be padding;
    union {
        u16 raw;

        BitField<0, 1, u16> gender;
        BitField<1, 4, u16> birth_month;
        BitField<5, 5, u16> birth_day;
        BitField<10, 4, u16> favorite_color;
        BitField<14, 1, u16> favorite;
    } mii_information;
    std::array<char16_t, 0xA> mii_name;
    u8 height;
    u8 build;
    union {
        u8 raw;

        BitField<0, 1, u8> disable_sharing;
        BitField<1, 4, u8> face_shape;
        BitField<5, 3, u8> skin_color;
    } appearance_bits1;
    union {
        u8 raw;

        BitField<0, 4, u8> wrinkles;
        BitField<4, 4, u8> makeup;
    } appearance_bits2;
    u8 hair_style;
    union {
        u8 raw;

        BitField<0, 3, u8> hair_color;
        BitField<3, 1, u8> flip_hair;
    } appearance_bits3;
    union {
        u32 raw;

        BitField<0, 6, u32> eye_type;
        BitField<6, 3, u32> eye_color;
        BitField<9, 4, u32> eye_scale;
        BitField<13, 3, u32> eye_vertical_stretch;
        BitField<16, 5, u32> eye_rotation;
        BitField<21, 4, u32> eye_spacing;
        BitField<25, 5, u32> eye_y_position;
    } appearance_bits4;
    union {
        u32 raw;

        BitField<0, 5, u32> eyebrow_style;
        BitField<5, 3, u32> eyebrow_color;
        BitField<8, 4, u32> eyebrow_scale;
        BitField<12, 3, u32> eyebrow_yscale;
        BitField<16, 4, u32> eyebrow_rotation;
        BitField<21, 4, u32> eyebrow_spacing;
        BitField<25, 5, u32> eyebrow_y_position;
    } appearance_bits5;
    union {
        u16 raw;

        BitField<0, 5, u16> nose_type;
        BitField<5, 4, u16> nose_scale;
        BitField<9, 5, u16> nose_y_position;
    } appearance_bits6;
    union {
        u16 raw;

        BitField<0, 6, u16> mouth_type;
        BitField<6, 3, u16> mouth_color;
        BitField<9, 4, u16> mouth_scale;
        BitField<13, 3, u16> mouth_horizontal_stretch;
    } appearance_bits7;
    union {
        u8 raw;

        BitField<0, 5, u8> mouth_y_position;
        BitField<5, 3, u8> mustache_type;
    } appearance_bits8;
    u8 allow_copying;
    union {
        u16 raw;

        BitField<0, 3, u16> bear_type;
        BitField<3, 3, u16> facial_hair_color;
        BitField<6, 4, u16> mustache_scale;
        BitField<10, 5, u16> mustache_y_position;
    } appearance_bits9;
    union {
        u16 raw;

        BitField<0, 4, u16> glasses_type;
        BitField<4, 3, u16> glasses_color;
        BitField<7, 4, u16> glasses_scale;
        BitField<11, 5, u16> glasses_y_position;
    } appearance_bits10;
    union {
        u16 raw;

        BitField<0, 1, u16> mole_enabled;
        BitField<1, 4, u16> mole_scale;
        BitField<5, 5, u16> mole_x_position;
        BitField<10, 5, u16> mole_y_position;
    } appearance_bits11;

    std::array<u16_le, 0xA> author_name;
    INSERT_PADDING_BYTES(0x2);
    u16_be crc;
};
static_assert(sizeof(Ver3StoreData) == 0x60, "Ver3StoreData is an invalid size");
#pragma pack(pop)

}; // namespace Service::Mii
