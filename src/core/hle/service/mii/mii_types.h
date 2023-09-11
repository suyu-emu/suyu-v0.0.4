// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <type_traits>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/uuid.h"

namespace Service::Mii {

enum class Age : u32 {
    Young,
    Normal,
    Old,
    All,
};

enum class BeardType : u32 {
    None,
    Beard1,
    Beard2,
    Beard3,
    Beard4,
    Beard5,
};

enum class BeardAndMustacheFlag : u32 {
    Beard = 1,
    Mustache,
    All = Beard | Mustache,
};
DECLARE_ENUM_FLAG_OPERATORS(BeardAndMustacheFlag);

enum class FontRegion : u32 {
    Standard,
    China,
    Korea,
    Taiwan,
};

enum class Gender : u32 {
    Male,
    Female,
    All,
    Maximum = Female,
};

enum class HairFlip : u32 {
    Left,
    Right,
    Maximum = Right,
};

enum class MustacheType : u32 {
    None,
    Mustache1,
    Mustache2,
    Mustache3,
    Mustache4,
    Mustache5,
};

enum class Race : u32 {
    Black,
    White,
    Asian,
    All,
};

enum class Source : u32 {
    Database = 0,
    Default = 1,
    Account = 2,
    Friend = 3,
};

enum class SourceFlag : u32 {
    None = 0,
    Database = 1 << 0,
    Default = 1 << 1,
};
DECLARE_ENUM_FLAG_OPERATORS(SourceFlag);

enum class ValidationResult : u32 {
    NoErrors = 0x0,
    InvalidBeardColor = 0x1,
    InvalidBeardType = 0x2,
    InvalidBuild = 0x3,
    InvalidEyeAspect = 0x4,
    InvalidEyeColor = 0x5,
    InvalidEyeRotate = 0x6,
    InvalidEyeScale = 0x7,
    InvalidEyeType = 0x8,
    InvalidEyeX = 0x9,
    InvalidEyeY = 0xa,
    InvalidEyebrowAspect = 0xb,
    InvalidEyebrowColor = 0xc,
    InvalidEyebrowRotate = 0xd,
    InvalidEyebrowScale = 0xe,
    InvalidEyebrowType = 0xf,
    InvalidEyebrowX = 0x10,
    InvalidEyebrowY = 0x11,
    InvalidFacelineColor = 0x12,
    InvalidFacelineMake = 0x13,
    InvalidFacelineWrinkle = 0x14,
    InvalidFacelineType = 0x15,
    InvalidColor = 0x16,
    InvalidFont = 0x17,
    InvalidGender = 0x18,
    InvalidGlassColor = 0x19,
    InvalidGlassScale = 0x1a,
    InvalidGlassType = 0x1b,
    InvalidGlassY = 0x1c,
    InvalidHairColor = 0x1d,
    InvalidHairFlip = 0x1e,
    InvalidHairType = 0x1f,
    InvalidHeight = 0x20,
    InvalidMoleScale = 0x21,
    InvalidMoleType = 0x22,
    InvalidMoleX = 0x23,
    InvalidMoleY = 0x24,
    InvalidMouthAspect = 0x25,
    InvalidMouthColor = 0x26,
    InvalidMouthScale = 0x27,
    InvalidMouthType = 0x28,
    InvalidMouthY = 0x29,
    InvalidMustacheScale = 0x2a,
    InvalidMustacheType = 0x2b,
    InvalidMustacheY = 0x2c,
    InvalidNoseScale = 0x2e,
    InvalidNoseType = 0x2f,
    InvalidNoseY = 0x30,
    InvalidRegionMove = 0x31,
    InvalidCreateId = 0x32,
    InvalidName = 0x33,
    InvalidType = 0x35,
};

struct Nickname {
    static constexpr std::size_t MaxNameSize = 10;
    std::array<char16_t, MaxNameSize> data;

    // Checks for null, non-zero terminated or dirty strings
    bool IsValid() const {
        if (data[0] == 0) {
            return false;
        }

        if (data[MaxNameSize] != 0) {
            return false;
        }
        std::size_t index = 1;
        while (data[index] != 0) {
            index++;
        }
        while (index < MaxNameSize && data[index] == 0) {
            index++;
        }
        return index == MaxNameSize;
    }
};
static_assert(sizeof(Nickname) == 0x14, "Nickname is an invalid size");

struct DefaultMii {
    u32 face_type{};
    u32 face_color{};
    u32 face_wrinkle{};
    u32 face_makeup{};
    u32 hair_type{};
    u32 hair_color{};
    HairFlip hair_flip{};
    u32 eye_type{};
    u32 eye_color{};
    u32 eye_scale{};
    u32 eye_aspect{};
    u32 eye_rotate{};
    u32 eye_x{};
    u32 eye_y{};
    u32 eyebrow_type{};
    u32 eyebrow_color{};
    u32 eyebrow_scale{};
    u32 eyebrow_aspect{};
    u32 eyebrow_rotate{};
    u32 eyebrow_x{};
    u32 eyebrow_y{};
    u32 nose_type{};
    u32 nose_scale{};
    u32 nose_y{};
    u32 mouth_type{};
    u32 mouth_color{};
    u32 mouth_scale{};
    u32 mouth_aspect{};
    u32 mouth_y{};
    MustacheType mustache_type{};
    BeardType beard_type{};
    u32 beard_color{};
    u32 mustache_scale{};
    u32 mustache_y{};
    u32 glasses_type{};
    u32 glasses_color{};
    u32 glasses_scale{};
    u32 glasses_y{};
    u32 mole_type{};
    u32 mole_scale{};
    u32 mole_x{};
    u32 mole_y{};
    u32 height{};
    u32 weight{};
    Gender gender{};
    u32 favorite_color{};
    u32 region_move{};
    FontRegion font_region{};
    u32 type{};
    Nickname nickname;
};
static_assert(sizeof(DefaultMii) == 0xd8, "DefaultMii has incorrect size.");

struct DatabaseSessionMetadata {
    u32 interface_version;
    u32 magic;
    u64 update_counter;

    bool IsInterfaceVersionSupported(u32 version) const {
        return version <= interface_version;
    }
};

} // namespace Service::Mii
