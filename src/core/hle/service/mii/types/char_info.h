// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"

namespace Service::Mii {
class StoreData;

// This is nn::mii::detail::CharInfoRaw
class CharInfo {
public:
    void SetFromStoreData(const StoreData& store_data_raw);

    u32 Verify() const;

    Common::UUID GetCreateId() const;
    Nickname GetNickname() const;
    u8 GetFontRegion() const;
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

    bool operator==(const CharInfo& info);

private:
    Common::UUID create_id;
    Nickname name;
    u16 null_terminator;
    u8 font_region;
    u8 favorite_color;
    u8 gender;
    u8 height;
    u8 build;
    u8 type;
    u8 region_move;
    u8 faceline_type;
    u8 faceline_color;
    u8 faceline_wrinkle;
    u8 faceline_make;
    u8 hair_type;
    u8 hair_color;
    u8 hair_flip;
    u8 eye_type;
    u8 eye_color;
    u8 eye_scale;
    u8 eye_aspect;
    u8 eye_rotate;
    u8 eye_x;
    u8 eye_y;
    u8 eyebrow_type;
    u8 eyebrow_color;
    u8 eyebrow_scale;
    u8 eyebrow_aspect;
    u8 eyebrow_rotate;
    u8 eyebrow_x;
    u8 eyebrow_y;
    u8 nose_type;
    u8 nose_scale;
    u8 nose_y;
    u8 mouth_type;
    u8 mouth_color;
    u8 mouth_scale;
    u8 mouth_aspect;
    u8 mouth_y;
    u8 beard_color;
    u8 beard_type;
    u8 mustache_type;
    u8 mustache_scale;
    u8 mustache_y;
    u8 glasses_type;
    u8 glasses_color;
    u8 glasses_scale;
    u8 glasses_y;
    u8 mole_type;
    u8 mole_scale;
    u8 mole_x;
    u8 mole_y;
    u8 padding;
};
static_assert(sizeof(CharInfo) == 0x58, "CharInfo has incorrect size.");
static_assert(std::has_unique_object_representations_v<CharInfo>,
              "All bits of CharInfo must contribute to its value.");

struct CharInfoElement {
    CharInfo char_info{};
    Source source{};
};
static_assert(sizeof(CharInfoElement) == 0x5c, "CharInfoElement has incorrect size.");

}; // namespace Service::Mii
