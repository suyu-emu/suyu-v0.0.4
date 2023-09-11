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
static_assert(sizeof(DefaultMii) == 0xd8, "MiiStoreData has incorrect size.");

struct DatabaseSessionMetadata {
    u32 interface_version;
    u32 magic;
    u64 update_counter;

    bool IsInterfaceVersionSupported(u32 version) const {
        return version <= interface_version;
    }
};

} // namespace Service::Mii
