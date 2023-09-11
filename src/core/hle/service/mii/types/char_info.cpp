// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

void CharInfo::SetFromStoreData(const StoreData& store_data) {
    name = store_data.GetNickname();
    null_terminator = '\0';
    create_id = store_data.GetCreateId();
    font_region = static_cast<u8>(store_data.GetFontRegion());
    favorite_color = store_data.GetFavoriteColor();
    gender = store_data.GetGender();
    height = store_data.GetHeight();
    build = store_data.GetBuild();
    type = store_data.GetType();
    region_move = store_data.GetRegionMove();
    faceline_type = store_data.GetFacelineType();
    faceline_color = store_data.GetFacelineColor();
    faceline_wrinkle = store_data.GetFacelineWrinkle();
    faceline_make = store_data.GetFacelineMake();
    hair_type = store_data.GetHairType();
    hair_color = store_data.GetHairColor();
    hair_flip = store_data.GetHairFlip();
    eye_type = store_data.GetEyeType();
    eye_color = store_data.GetEyeColor();
    eye_scale = store_data.GetEyeScale();
    eye_aspect = store_data.GetEyeAspect();
    eye_rotate = store_data.GetEyeRotate();
    eye_x = store_data.GetEyeX();
    eye_y = store_data.GetEyeY();
    eyebrow_type = store_data.GetEyebrowType();
    eyebrow_color = store_data.GetEyebrowColor();
    eyebrow_scale = store_data.GetEyebrowScale();
    eyebrow_aspect = store_data.GetEyebrowAspect();
    eyebrow_rotate = store_data.GetEyebrowRotate();
    eyebrow_x = store_data.GetEyebrowX();
    eyebrow_y = store_data.GetEyebrowY();
    nose_type = store_data.GetNoseType();
    nose_scale = store_data.GetNoseScale();
    nose_y = store_data.GetNoseY();
    mouth_type = store_data.GetMouthType();
    mouth_color = store_data.GetMouthColor();
    mouth_scale = store_data.GetMouthScale();
    mouth_aspect = store_data.GetMouthAspect();
    mouth_y = store_data.GetMouthY();
    beard_color = store_data.GetBeardColor();
    beard_type = store_data.GetBeardType();
    mustache_type = store_data.GetMustacheType();
    mustache_scale = store_data.GetMustacheScale();
    mustache_y = store_data.GetMustacheY();
    glasses_type = store_data.GetGlassType();
    glasses_color = store_data.GetGlassColor();
    glasses_scale = store_data.GetGlassScale();
    glasses_y = store_data.GetGlassY();
    mole_type = store_data.GetMoleType();
    mole_scale = store_data.GetMoleScale();
    mole_x = store_data.GetMoleX();
    mole_y = store_data.GetMoleY();
    padding = '\0';
}

u32 CharInfo::Verify() const {
    if (!create_id.IsValid()) {
        return 0x32;
    }
    if (!name.IsValid()) {
        return 0x33;
    }
    if (3 < font_region) {
        return 0x17;
    }
    if (0xb < favorite_color) {
        return 0x16;
    }
    if (1 < gender) {
        return 0x18;
    }
    if (height < 0) {
        return 0x20;
    }
    if (build < 0) {
        return 3;
    }
    if (1 < type) {
        return 0x35;
    }
    if (3 < region_move) {
        return 0x31;
    }
    if (0xb < faceline_type) {
        return 0x15;
    }
    if (9 < faceline_color) {
        return 0x12;
    }
    if (0xb < faceline_wrinkle) {
        return 0x14;
    }
    if (0xb < faceline_make) {
        return 0x13;
    }
    if (0x83 < hair_type) {
        return 0x1f;
    }
    if (99 < hair_color) {
        return 0x1d;
    }
    if (1 < hair_flip) {
        return 0x1e;
    }
    if (0x3b < eye_type) {
        return 8;
    }
    if (99 < eye_color) {
        return 5;
    }
    if (7 < eye_scale) {
        return 7;
    }
    if (6 < eye_aspect) {
        return 4;
    }
    if (7 < eye_rotate) {
        return 6;
    }
    if (0xc < eye_x) {
        return 9;
    }
    if (0x12 < eye_y) {
        return 10;
    }
    if (0x17 < eyebrow_type) {
        return 0xf;
    }
    if (99 < eyebrow_color) {
        return 0xc;
    }
    if (8 < eyebrow_scale) {
        return 0xe;
    }
    if (6 < eyebrow_aspect) {
        return 0xb;
    }
    if (0xb < eyebrow_rotate) {
        return 0xd;
    }
    if (0xc < eyebrow_x) {
        return 0x10;
    }
    if (0xf < eyebrow_y - 3) {
        return 0x11;
    }
    if (0x11 < nose_type) {
        return 0x2f;
    }
    if (nose_scale >= 9) {
        return 0x2e;
    }
    if (0x12 < nose_y) {
        return 0x30;
    }
    if (0x23 < mouth_type) {
        return 0x28;
    }
    if (99 < mouth_color) {
        return 0x26;
    }
    if (8 < mouth_scale) {
        return 0x27;
    }
    if (6 < mouth_aspect) {
        return 0x25;
    }
    if (0x12 < mouth_y) {
        return 0x29;
    }
    if (99 < beard_color) {
        return 1;
    }
    if (5 < beard_type) {
        return 2;
    }
    if (5 < mustache_type) {
        return 0x2b;
    }
    if (8 < mustache_scale) {
        return 0x2a;
    }
    if (0x10 < mustache_y) {
        return 0x2c;
    }
    if (0x13 < glasses_type) {
        return 0x1b;
    }
    if (99 < glasses_color) {
        return 0x19;
    }
    if (7 < glasses_scale) {
        return 0x1a;
    }
    if (0x14 < glasses_y) {
        return 0x1c;
    }
    if (mole_type >= 2) {
        return 0x22;
    }
    if (8 < mole_scale) {
        return 0x21;
    }
    if (mole_x >= 0x11) {
        return 0x23;
    }
    if (0x1e < mole_y) {
        return 0x24;
    }
    return 0;
}

Common::UUID CharInfo::GetCreateId() const {
    return create_id;
}

Nickname CharInfo::GetNickname() const {
    return name;
}

u8 CharInfo::GetFontRegion() const {
    return font_region;
}

u8 CharInfo::GetFavoriteColor() const {
    return favorite_color;
}

u8 CharInfo::GetGender() const {
    return gender;
}

u8 CharInfo::GetHeight() const {
    return height;
}

u8 CharInfo::GetBuild() const {
    return build;
}

u8 CharInfo::GetType() const {
    return type;
}

u8 CharInfo::GetRegionMove() const {
    return region_move;
}

u8 CharInfo::GetFacelineType() const {
    return faceline_type;
}

u8 CharInfo::GetFacelineColor() const {
    return faceline_color;
}

u8 CharInfo::GetFacelineWrinkle() const {
    return faceline_wrinkle;
}

u8 CharInfo::GetFacelineMake() const {
    return faceline_make;
}

u8 CharInfo::GetHairType() const {
    return hair_type;
}

u8 CharInfo::GetHairColor() const {
    return hair_color;
}

u8 CharInfo::GetHairFlip() const {
    return hair_flip;
}

u8 CharInfo::GetEyeType() const {
    return eye_type;
}

u8 CharInfo::GetEyeColor() const {
    return eye_color;
}

u8 CharInfo::GetEyeScale() const {
    return eye_scale;
}

u8 CharInfo::GetEyeAspect() const {
    return eye_aspect;
}

u8 CharInfo::GetEyeRotate() const {
    return eye_rotate;
}

u8 CharInfo::GetEyeX() const {
    return eye_x;
}

u8 CharInfo::GetEyeY() const {
    return eye_y;
}

u8 CharInfo::GetEyebrowType() const {
    return eyebrow_type;
}

u8 CharInfo::GetEyebrowColor() const {
    return eyebrow_color;
}

u8 CharInfo::GetEyebrowScale() const {
    return eyebrow_scale;
}

u8 CharInfo::GetEyebrowAspect() const {
    return eyebrow_aspect;
}

u8 CharInfo::GetEyebrowRotate() const {
    return eyebrow_rotate;
}

u8 CharInfo::GetEyebrowX() const {
    return eyebrow_x;
}

u8 CharInfo::GetEyebrowY() const {
    return eyebrow_y;
}

u8 CharInfo::GetNoseType() const {
    return nose_type;
}

u8 CharInfo::GetNoseScale() const {
    return nose_scale;
}

u8 CharInfo::GetNoseY() const {
    return nose_y;
}

u8 CharInfo::GetMouthType() const {
    return mouth_type;
}

u8 CharInfo::GetMouthColor() const {
    return mouth_color;
}

u8 CharInfo::GetMouthScale() const {
    return mouth_scale;
}

u8 CharInfo::GetMouthAspect() const {
    return mouth_aspect;
}

u8 CharInfo::GetMouthY() const {
    return mouth_y;
}

u8 CharInfo::GetBeardColor() const {
    return beard_color;
}

u8 CharInfo::GetBeardType() const {
    return beard_type;
}

u8 CharInfo::GetMustacheType() const {
    return mustache_type;
}

u8 CharInfo::GetMustacheScale() const {
    return mustache_scale;
}

u8 CharInfo::GetMustacheY() const {
    return mustache_y;
}

u8 CharInfo::GetGlassType() const {
    return glasses_type;
}

u8 CharInfo::GetGlassColor() const {
    return glasses_color;
}

u8 CharInfo::GetGlassScale() const {
    return glasses_scale;
}

u8 CharInfo::GetGlassY() const {
    return glasses_y;
}

u8 CharInfo::GetMoleType() const {
    return mole_type;
}

u8 CharInfo::GetMoleScale() const {
    return mole_scale;
}

u8 CharInfo::GetMoleX() const {
    return mole_x;
}

u8 CharInfo::GetMoleY() const {
    return mole_y;
}

bool CharInfo::operator==(const CharInfo& info) {
    bool is_identical = info.Verify() == 0;
    is_identical &= name.data == info.GetNickname().data;
    is_identical &= create_id == info.GetCreateId();
    is_identical &= font_region == info.GetFontRegion();
    is_identical &= favorite_color == info.GetFavoriteColor();
    is_identical &= gender == info.GetGender();
    is_identical &= height == info.GetHeight();
    is_identical &= build == info.GetBuild();
    is_identical &= type == info.GetType();
    is_identical &= region_move == info.GetRegionMove();
    is_identical &= faceline_type == info.GetFacelineType();
    is_identical &= faceline_color == info.GetFacelineColor();
    is_identical &= faceline_wrinkle == info.GetFacelineWrinkle();
    is_identical &= faceline_make == info.GetFacelineMake();
    is_identical &= hair_type == info.GetHairType();
    is_identical &= hair_color == info.GetHairColor();
    is_identical &= hair_flip == info.GetHairFlip();
    is_identical &= eye_type == info.GetEyeType();
    is_identical &= eye_color == info.GetEyeColor();
    is_identical &= eye_scale == info.GetEyeScale();
    is_identical &= eye_aspect == info.GetEyeAspect();
    is_identical &= eye_rotate == info.GetEyeRotate();
    is_identical &= eye_x == info.GetEyeX();
    is_identical &= eye_y == info.GetEyeY();
    is_identical &= eyebrow_type == info.GetEyebrowType();
    is_identical &= eyebrow_color == info.GetEyebrowColor();
    is_identical &= eyebrow_scale == info.GetEyebrowScale();
    is_identical &= eyebrow_aspect == info.GetEyebrowAspect();
    is_identical &= eyebrow_rotate == info.GetEyebrowRotate();
    is_identical &= eyebrow_x == info.GetEyebrowX();
    is_identical &= eyebrow_y == info.GetEyebrowY();
    is_identical &= nose_type == info.GetNoseType();
    is_identical &= nose_scale == info.GetNoseScale();
    is_identical &= nose_y == info.GetNoseY();
    is_identical &= mouth_type == info.GetMouthType();
    is_identical &= mouth_color == info.GetMouthColor();
    is_identical &= mouth_scale == info.GetMouthScale();
    is_identical &= mouth_aspect == info.GetMouthAspect();
    is_identical &= mouth_y == info.GetMouthY();
    is_identical &= beard_color == info.GetBeardColor();
    is_identical &= beard_type == info.GetBeardType();
    is_identical &= mustache_type == info.GetMustacheType();
    is_identical &= mustache_scale == info.GetMustacheScale();
    is_identical &= mustache_y == info.GetMustacheY();
    is_identical &= glasses_type == info.GetGlassType();
    is_identical &= glasses_color == info.GetGlassColor();
    is_identical &= glasses_scale == info.GetGlassScale();
    is_identical &= glasses_y == info.GetGlassY();
    is_identical &= mole_type == info.GetMoleType();
    is_identical &= mole_scale == info.GetMoleScale();
    is_identical &= mole_x == info.GetMoleX();
    is_identical &= mole_y == info.GetMoleY();
    return is_identical;
}

} // namespace Service::Mii
