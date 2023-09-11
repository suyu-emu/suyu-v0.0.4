// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/raw_data.h"
#include "core/hle/service/mii/types/store_data.h"

namespace Service::Mii {

void StoreData::BuildDefault(u32 mii_index) {
    const auto& default_mii = RawData::DefaultMii[mii_index];
    core_data.SetDefault();

    core_data.SetFacelineType(static_cast<u8>(default_mii.face_type));
    core_data.SetFacelineColor(
        RawData::GetFacelineColorFromVer3(static_cast<u8>(default_mii.face_color)));
    core_data.SetFacelineWrinkle(static_cast<u8>(default_mii.face_wrinkle));
    core_data.SetFacelineMake(static_cast<u8>(default_mii.face_makeup));

    core_data.SetHairType(static_cast<u8>(default_mii.hair_type));
    core_data.SetHairColor(RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.hair_color)));
    core_data.SetHairFlip(default_mii.hair_flip);
    core_data.SetEyeType(static_cast<u8>(default_mii.eye_type));
    core_data.SetEyeColor(RawData::GetEyeColorFromVer3(static_cast<u8>(default_mii.eye_color)));
    core_data.SetEyeScale(static_cast<u8>(default_mii.eye_scale));
    core_data.SetEyeAspect(static_cast<u8>(default_mii.eye_aspect));
    core_data.SetEyeRotate(static_cast<u8>(default_mii.eye_rotate));
    core_data.SetEyeX(static_cast<u8>(default_mii.eye_x));
    core_data.SetEyeY(static_cast<u8>(default_mii.eye_y));

    core_data.SetEyebrowType(static_cast<u8>(default_mii.eyebrow_type));
    core_data.SetEyebrowColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.eyebrow_color)));
    core_data.SetEyebrowScale(static_cast<u8>(default_mii.eyebrow_scale));
    core_data.SetEyebrowAspect(static_cast<u8>(default_mii.eyebrow_aspect));
    core_data.SetEyebrowRotate(static_cast<u8>(default_mii.eyebrow_rotate));
    core_data.SetEyebrowX(static_cast<u8>(default_mii.eyebrow_x));
    core_data.SetEyebrowY(static_cast<u8>(default_mii.eyebrow_y));

    core_data.SetNoseType(static_cast<u8>(default_mii.nose_type));
    core_data.SetNoseScale(static_cast<u8>(default_mii.nose_scale));
    core_data.SetNoseY(static_cast<u8>(default_mii.nose_y));

    core_data.SetMouthType(static_cast<u8>(default_mii.mouth_type));
    core_data.SetMouthColor(
        RawData::GetMouthColorFromVer3(static_cast<u8>(default_mii.mouth_color)));
    core_data.SetMouthScale(static_cast<u8>(default_mii.mouth_scale));
    core_data.SetMouthAspect(static_cast<u8>(default_mii.mouth_aspect));
    core_data.SetMouthY(static_cast<u8>(default_mii.mouth_y));

    core_data.SetMustacheType(default_mii.mustache_type);
    core_data.SetBeardType(default_mii.beard_type);
    core_data.SetBeardColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.beard_color)));
    core_data.SetMustacheScale(static_cast<u8>(default_mii.mustache_scale));
    core_data.SetMustacheY(static_cast<u8>(default_mii.mustache_y));

    core_data.SetGlassType(static_cast<u8>(default_mii.glasses_type));
    core_data.SetGlassColor(
        RawData::GetGlassColorFromVer3(static_cast<u8>(default_mii.glasses_color)));
    core_data.SetGlassScale(static_cast<u8>(default_mii.glasses_scale));
    core_data.SetGlassY(static_cast<u8>(default_mii.glasses_y));

    core_data.SetMoleType(static_cast<u8>(default_mii.mole_type));
    core_data.SetMoleScale(static_cast<u8>(default_mii.mole_scale));
    core_data.SetMoleX(static_cast<u8>(default_mii.mole_x));
    core_data.SetMoleY(static_cast<u8>(default_mii.mole_y));

    core_data.SetHeight(static_cast<u8>(default_mii.height));
    core_data.SetBuild(static_cast<u8>(default_mii.weight));
    core_data.SetGender(default_mii.gender);
    core_data.SetFavoriteColor(static_cast<u8>(default_mii.favorite_color));
    core_data.SetRegionMove(static_cast<u8>(default_mii.region_move));
    core_data.SetFontRegion(default_mii.font_region);
    core_data.SetType(static_cast<u8>(default_mii.type));
    core_data.SetNickname(default_mii.nickname);

    const auto device_id = MiiUtil::GetDeviceId();
    create_id = MiiUtil::MakeCreateId();
    device_crc = MiiUtil::CalculateCrc16(&device_id, sizeof(Common::UUID));
    data_crc = MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData));
}

void StoreData::BuildBase(Gender gender) {
    const auto& default_mii = RawData::BaseMii[gender == Gender::Female ? 1 : 0];
    core_data.SetDefault();

    core_data.SetFacelineType(static_cast<u8>(default_mii.face_type));
    core_data.SetFacelineColor(
        RawData::GetFacelineColorFromVer3(static_cast<u8>(default_mii.face_color)));
    core_data.SetFacelineWrinkle(static_cast<u8>(default_mii.face_wrinkle));
    core_data.SetFacelineMake(static_cast<u8>(default_mii.face_makeup));

    core_data.SetHairType(static_cast<u8>(default_mii.hair_type));
    core_data.SetHairColor(RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.hair_color)));
    core_data.SetHairFlip(default_mii.hair_flip);
    core_data.SetEyeType(static_cast<u8>(default_mii.eye_type));
    core_data.SetEyeColor(RawData::GetEyeColorFromVer3(static_cast<u8>(default_mii.eye_color)));
    core_data.SetEyeScale(static_cast<u8>(default_mii.eye_scale));
    core_data.SetEyeAspect(static_cast<u8>(default_mii.eye_aspect));
    core_data.SetEyeRotate(static_cast<u8>(default_mii.eye_rotate));
    core_data.SetEyeX(static_cast<u8>(default_mii.eye_x));
    core_data.SetEyeY(static_cast<u8>(default_mii.eye_y));

    core_data.SetEyebrowType(static_cast<u8>(default_mii.eyebrow_type));
    core_data.SetEyebrowColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.eyebrow_color)));
    core_data.SetEyebrowScale(static_cast<u8>(default_mii.eyebrow_scale));
    core_data.SetEyebrowAspect(static_cast<u8>(default_mii.eyebrow_aspect));
    core_data.SetEyebrowRotate(static_cast<u8>(default_mii.eyebrow_rotate));
    core_data.SetEyebrowX(static_cast<u8>(default_mii.eyebrow_x));
    core_data.SetEyebrowY(static_cast<u8>(default_mii.eyebrow_y));

    core_data.SetNoseType(static_cast<u8>(default_mii.nose_type));
    core_data.SetNoseScale(static_cast<u8>(default_mii.nose_scale));
    core_data.SetNoseY(static_cast<u8>(default_mii.nose_y));

    core_data.SetMouthType(static_cast<u8>(default_mii.mouth_type));
    core_data.SetMouthColor(
        RawData::GetMouthColorFromVer3(static_cast<u8>(default_mii.mouth_color)));
    core_data.SetMouthScale(static_cast<u8>(default_mii.mouth_scale));
    core_data.SetMouthAspect(static_cast<u8>(default_mii.mouth_aspect));
    core_data.SetMouthY(static_cast<u8>(default_mii.mouth_y));

    core_data.SetMustacheType(default_mii.mustache_type);
    core_data.SetBeardType(default_mii.beard_type);
    core_data.SetBeardColor(
        RawData::GetHairColorFromVer3(static_cast<u8>(default_mii.beard_color)));
    core_data.SetMustacheScale(static_cast<u8>(default_mii.mustache_scale));
    core_data.SetMustacheY(static_cast<u8>(default_mii.mustache_y));

    core_data.SetGlassType(static_cast<u8>(default_mii.glasses_type));
    core_data.SetGlassColor(
        RawData::GetGlassColorFromVer3(static_cast<u8>(default_mii.glasses_color)));
    core_data.SetGlassScale(static_cast<u8>(default_mii.glasses_scale));
    core_data.SetGlassY(static_cast<u8>(default_mii.glasses_y));

    core_data.SetMoleType(static_cast<u8>(default_mii.mole_type));
    core_data.SetMoleScale(static_cast<u8>(default_mii.mole_scale));
    core_data.SetMoleX(static_cast<u8>(default_mii.mole_x));
    core_data.SetMoleY(static_cast<u8>(default_mii.mole_y));

    core_data.SetHeight(static_cast<u8>(default_mii.height));
    core_data.SetBuild(static_cast<u8>(default_mii.weight));
    core_data.SetGender(default_mii.gender);
    core_data.SetFavoriteColor(static_cast<u8>(default_mii.favorite_color));
    core_data.SetRegionMove(static_cast<u8>(default_mii.region_move));
    core_data.SetFontRegion(default_mii.font_region);
    core_data.SetType(static_cast<u8>(default_mii.type));
    core_data.SetNickname(default_mii.nickname);

    const auto device_id = MiiUtil::GetDeviceId();
    create_id = MiiUtil::MakeCreateId();
    device_crc = MiiUtil::CalculateCrc16(&device_id, sizeof(Common::UUID));
    data_crc = MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData));
}

void StoreData::BuildRandom(Age age, Gender gender, Race race) {
    core_data.BuildRandom(age, gender, race);
    const auto device_id = MiiUtil::GetDeviceId();
    create_id = MiiUtil::MakeCreateId();
    device_crc = MiiUtil::CalculateCrc16(&device_id, sizeof(Common::UUID));
    data_crc = MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData));
}

void StoreData::SetInvalidName() {
    const auto& invalid_name = core_data.GetInvalidNickname();
    const auto device_id = MiiUtil::GetDeviceId();
    core_data.SetNickname(invalid_name);
    device_crc = MiiUtil::CalculateCrc16(&device_id, sizeof(Common::UUID));
    data_crc = MiiUtil::CalculateCrc16(&core_data, sizeof(CoreData));
}

bool StoreData::IsSpecial() const {
    return GetType() == 1;
}

u32 StoreData::IsValid() const {
    // TODO: complete this
    return 0;
}

Common::UUID StoreData::GetCreateId() const {
    return create_id;
}

FontRegion StoreData::GetFontRegion() const {
    return static_cast<FontRegion>(core_data.GetFontRegion());
}

u8 StoreData::GetFavoriteColor() const {
    return core_data.GetFavoriteColor();
}

u8 StoreData::GetGender() const {
    return core_data.GetGender();
}

u8 StoreData::GetHeight() const {
    return core_data.GetHeight();
}

u8 StoreData::GetBuild() const {
    return core_data.GetBuild();
}

u8 StoreData::GetType() const {
    return core_data.GetType();
}

u8 StoreData::GetRegionMove() const {
    return core_data.GetRegionMove();
}

u8 StoreData::GetFacelineType() const {
    return core_data.GetFacelineType();
}

u8 StoreData::GetFacelineColor() const {
    return core_data.GetFacelineColor();
}

u8 StoreData::GetFacelineWrinkle() const {
    return core_data.GetFacelineWrinkle();
}

u8 StoreData::GetFacelineMake() const {
    return core_data.GetFacelineMake();
}

u8 StoreData::GetHairType() const {
    return core_data.GetHairType();
}

u8 StoreData::GetHairColor() const {
    return core_data.GetHairColor();
}

u8 StoreData::GetHairFlip() const {
    return core_data.GetHairFlip();
}

u8 StoreData::GetEyeType() const {
    return core_data.GetEyeType();
}

u8 StoreData::GetEyeColor() const {
    return core_data.GetEyeColor();
}

u8 StoreData::GetEyeScale() const {
    return core_data.GetEyeScale();
}

u8 StoreData::GetEyeAspect() const {
    return core_data.GetEyeAspect();
}

u8 StoreData::GetEyeRotate() const {
    return core_data.GetEyeRotate();
}

u8 StoreData::GetEyeX() const {
    return core_data.GetEyeX();
}

u8 StoreData::GetEyeY() const {
    return core_data.GetEyeY();
}

u8 StoreData::GetEyebrowType() const {
    return core_data.GetEyebrowType();
}

u8 StoreData::GetEyebrowColor() const {
    return core_data.GetEyebrowColor();
}

u8 StoreData::GetEyebrowScale() const {
    return core_data.GetEyebrowScale();
}

u8 StoreData::GetEyebrowAspect() const {
    return core_data.GetEyebrowAspect();
}

u8 StoreData::GetEyebrowRotate() const {
    return core_data.GetEyebrowRotate();
}

u8 StoreData::GetEyebrowX() const {
    return core_data.GetEyebrowX();
}

u8 StoreData::GetEyebrowY() const {
    return core_data.GetEyebrowY();
}

u8 StoreData::GetNoseType() const {
    return core_data.GetNoseType();
}

u8 StoreData::GetNoseScale() const {
    return core_data.GetNoseScale();
}

u8 StoreData::GetNoseY() const {
    return core_data.GetNoseY();
}

u8 StoreData::GetMouthType() const {
    return core_data.GetMouthType();
}

u8 StoreData::GetMouthColor() const {
    return core_data.GetMouthColor();
}

u8 StoreData::GetMouthScale() const {
    return core_data.GetMouthScale();
}

u8 StoreData::GetMouthAspect() const {
    return core_data.GetMouthAspect();
}

u8 StoreData::GetMouthY() const {
    return core_data.GetMouthY();
}

u8 StoreData::GetBeardColor() const {
    return core_data.GetBeardColor();
}

u8 StoreData::GetBeardType() const {
    return core_data.GetBeardType();
}

u8 StoreData::GetMustacheType() const {
    return core_data.GetMustacheType();
}

u8 StoreData::GetMustacheScale() const {
    return core_data.GetMustacheScale();
}

u8 StoreData::GetMustacheY() const {
    return core_data.GetMustacheY();
}

u8 StoreData::GetGlassType() const {
    return core_data.GetGlassType();
}

u8 StoreData::GetGlassColor() const {
    return core_data.GetGlassColor();
}

u8 StoreData::GetGlassScale() const {
    return core_data.GetGlassScale();
}

u8 StoreData::GetGlassY() const {
    return core_data.GetGlassY();
}

u8 StoreData::GetMoleType() const {
    return core_data.GetMoleType();
}

u8 StoreData::GetMoleScale() const {
    return core_data.GetMoleScale();
}

u8 StoreData::GetMoleX() const {
    return core_data.GetMoleX();
}

u8 StoreData::GetMoleY() const {
    return core_data.GetMoleY();
}

Nickname StoreData::GetNickname() const {
    return core_data.GetNickname();
}

bool StoreData::operator==(const StoreData& data) {
    bool is_identical = data.core_data.IsValid() == 0;
    is_identical &= core_data.GetNickname() == data.core_data.GetNickname();
    is_identical &= GetCreateId() == data.GetCreateId();
    is_identical &= GetFontRegion() == data.GetFontRegion();
    is_identical &= GetFavoriteColor() == data.GetFavoriteColor();
    is_identical &= GetGender() == data.GetGender();
    is_identical &= GetHeight() == data.GetHeight();
    is_identical &= GetBuild() == data.GetBuild();
    is_identical &= GetType() == data.GetType();
    is_identical &= GetRegionMove() == data.GetRegionMove();
    is_identical &= GetFacelineType() == data.GetFacelineType();
    is_identical &= GetFacelineColor() == data.GetFacelineColor();
    is_identical &= GetFacelineWrinkle() == data.GetFacelineWrinkle();
    is_identical &= GetFacelineMake() == data.GetFacelineMake();
    is_identical &= GetHairType() == data.GetHairType();
    is_identical &= GetHairColor() == data.GetHairColor();
    is_identical &= GetHairFlip() == data.GetHairFlip();
    is_identical &= GetEyeType() == data.GetEyeType();
    is_identical &= GetEyeColor() == data.GetEyeColor();
    is_identical &= GetEyeScale() == data.GetEyeScale();
    is_identical &= GetEyeAspect() == data.GetEyeAspect();
    is_identical &= GetEyeRotate() == data.GetEyeRotate();
    is_identical &= GetEyeX() == data.GetEyeX();
    is_identical &= GetEyeY() == data.GetEyeY();
    is_identical &= GetEyebrowType() == data.GetEyebrowType();
    is_identical &= GetEyebrowColor() == data.GetEyebrowColor();
    is_identical &= GetEyebrowScale() == data.GetEyebrowScale();
    is_identical &= GetEyebrowAspect() == data.GetEyebrowAspect();
    is_identical &= GetEyebrowRotate() == data.GetEyebrowRotate();
    is_identical &= GetEyebrowX() == data.GetEyebrowX();
    is_identical &= GetEyebrowY() == data.GetEyebrowY();
    is_identical &= GetNoseType() == data.GetNoseType();
    is_identical &= GetNoseScale() == data.GetNoseScale();
    is_identical &= GetNoseY() == data.GetNoseY();
    is_identical &= GetMouthType() == data.GetMouthType();
    is_identical &= GetMouthColor() == data.GetMouthColor();
    is_identical &= GetMouthScale() == data.GetMouthScale();
    is_identical &= GetMouthAspect() == data.GetMouthAspect();
    is_identical &= GetMouthY() == data.GetMouthY();
    is_identical &= GetBeardColor() == data.GetBeardColor();
    is_identical &= GetBeardType() == data.GetBeardType();
    is_identical &= GetMustacheType() == data.GetMustacheType();
    is_identical &= GetMustacheScale() == data.GetMustacheScale();
    is_identical &= GetMustacheY() == data.GetMustacheY();
    is_identical &= GetGlassType() == data.GetGlassType();
    is_identical &= GetGlassColor() == data.GetGlassColor();
    is_identical &= GetGlassScale() == data.GetGlassScale();
    is_identical &= GetGlassY() == data.GetGlassY();
    is_identical &= GetMoleType() == data.GetMoleType();
    is_identical &= GetMoleScale() == data.GetMoleScale();
    is_identical &= GetMoleX() == data.GetMoleX();
    is_identical &= data.GetMoleY() == data.GetMoleY();
    return is_identical;
}

} // namespace Service::Mii
