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

class CoreData {
public:
    void SetDefault();
    void BuildRandom(Age age, Gender gender, Race race);

    u32 IsValid() const;

    void SetFontRegion(FontRegion value);
    void SetFavoriteColor(u8 value);
    void SetGender(Gender value);
    void SetHeight(u8 value);
    void SetBuild(u8 value);
    void SetType(u8 value);
    void SetRegionMove(u8 value);
    void SetFacelineType(u8 value);
    void SetFacelineColor(u8 value);
    void SetFacelineWrinkle(u8 value);
    void SetFacelineMake(u8 value);
    void SetHairType(u8 value);
    void SetHairColor(u8 value);
    void SetHairFlip(HairFlip value);
    void SetEyeType(u8 value);
    void SetEyeColor(u8 value);
    void SetEyeScale(u8 value);
    void SetEyeAspect(u8 value);
    void SetEyeRotate(u8 value);
    void SetEyeX(u8 value);
    void SetEyeY(u8 value);
    void SetEyebrowType(u8 value);
    void SetEyebrowColor(u8 value);
    void SetEyebrowScale(u8 value);
    void SetEyebrowAspect(u8 value);
    void SetEyebrowRotate(u8 value);
    void SetEyebrowX(u8 value);
    void SetEyebrowY(u8 value);
    void SetNoseType(u8 value);
    void SetNoseScale(u8 value);
    void SetNoseY(u8 value);
    void SetMouthType(u8 value);
    void SetMouthColor(u8 value);
    void SetMouthScale(u8 value);
    void SetMouthAspect(u8 value);
    void SetMouthY(u8 value);
    void SetBeardColor(u8 value);
    void SetBeardType(BeardType value);
    void SetMustacheType(MustacheType value);
    void SetMustacheScale(u8 value);
    void SetMustacheY(u8 value);
    void SetGlassType(u8 value);
    void SetGlassColor(u8 value);
    void SetGlassScale(u8 value);
    void SetGlassY(u8 value);
    void SetMoleType(u8 value);
    void SetMoleScale(u8 value);
    void SetMoleX(u8 value);
    void SetMoleY(u8 value);
    void SetNickname(Nickname nickname);

    u8 GetFavoriteColor() const;
    u8 GetGender() const;
    u8 GetHeight() const;
    u8 GetBuild() const;
    u8 GetType() const;
    u8 GetRegionMove() const;
    u8 GetFacelineType() const;
    u8 GetFacelineColor() const;
    u8 GetFacelineWrinkle() const;
    u8 GetFacelineMake() const;
    u8 GetHairType() const;
    u8 GetHairColor() const;
    u8 GetHairFlip() const;
    u8 GetEyeType() const;
    u8 GetEyeColor() const;
    u8 GetEyeScale() const;
    u8 GetEyeAspect() const;
    u8 GetEyeRotate() const;
    u8 GetEyeX() const;
    u8 GetEyeY() const;
    u8 GetEyebrowType() const;
    u8 GetEyebrowColor() const;
    u8 GetEyebrowScale() const;
    u8 GetEyebrowAspect() const;
    u8 GetEyebrowRotate() const;
    u8 GetEyebrowX() const;
    u8 GetEyebrowY() const;
    u8 GetNoseType() const;
    u8 GetNoseScale() const;
    u8 GetNoseY() const;
    u8 GetMouthType() const;
    u8 GetMouthColor() const;
    u8 GetMouthScale() const;
    u8 GetMouthAspect() const;
    u8 GetMouthY() const;
    u8 GetBeardColor() const;
    u8 GetBeardType() const;
    u8 GetMustacheType() const;
    u8 GetMustacheScale() const;
    u8 GetMustacheY() const;
    u8 GetGlassType() const;
    u8 GetGlassColor() const;
    u8 GetGlassScale() const;
    u8 GetGlassY() const;
    u8 GetMoleType() const;
    u8 GetMoleScale() const;
    u8 GetMoleX() const;
    u8 GetMoleY() const;
    Nickname GetNickname() const;
    Nickname GetDefaultNickname() const;
    Nickname GetInvalidNickname() const;

    StoreDataBitFields data{};
    Nickname name{};
};
static_assert(sizeof(CoreData) == 0x30, "CoreData has incorrect size.");

}; // namespace Service::Mii
