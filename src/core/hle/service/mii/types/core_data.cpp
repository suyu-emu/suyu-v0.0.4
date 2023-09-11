// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/core_data.h"
#include "core/hle/service/mii/types/raw_data.h"

namespace Service::Mii {

void CoreData::SetDefault() {
    data = {};
    name = GetDefaultNickname();
}

void CoreData::BuildRandom(Age age, Gender gender, Race race) {
    if (gender == Gender::All) {
        gender = MiiUtil::GetRandomValue<Gender>(Gender::Maximum);
    }

    data.gender.Assign(gender);
    data.favorite_color.Assign(MiiUtil::GetRandomValue<u8>(11));
    data.region_move.Assign(0);
    data.font_region.Assign(FontRegion::Standard);
    data.type.Assign(0);
    data.height.Assign(64);
    data.build.Assign(64);

    if (age == Age::All) {
        const auto temp{MiiUtil::GetRandomValue<int>(10)};
        if (temp >= 8) {
            age = Age::Old;
        } else if (temp >= 4) {
            age = Age::Normal;
        } else {
            age = Age::Young;
        }
    }

    if (race == Race::All) {
        const auto temp{MiiUtil::GetRandomValue<int>(10)};
        if (temp >= 8) {
            race = Race::Black;
        } else if (temp >= 4) {
            race = Race::White;
        } else {
            race = Race::Asian;
        }
    }

    u32 axis_y{};
    if (gender == Gender::Female && age == Age::Young) {
        axis_y = MiiUtil::GetRandomValue<u32>(3);
    }

    const std::size_t index{3 * static_cast<std::size_t>(age) +
                            9 * static_cast<std::size_t>(gender) + static_cast<std::size_t>(race)};

    const auto& faceline_type_info{RawData::RandomMiiFaceline.at(index)};
    const auto& faceline_color_info{RawData::RandomMiiFacelineColor.at(
        3 * static_cast<std::size_t>(gender) + static_cast<std::size_t>(race))};
    const auto& faceline_wrinkle_info{RawData::RandomMiiFacelineWrinkle.at(index)};
    const auto& faceline_makeup_info{RawData::RandomMiiFacelineMakeup.at(index)};
    const auto& hair_type_info{RawData::RandomMiiHairType.at(index)};
    const auto& hair_color_info{RawData::RandomMiiHairColor.at(3 * static_cast<std::size_t>(race) +
                                                               static_cast<std::size_t>(age))};
    const auto& eye_type_info{RawData::RandomMiiEyeType.at(index)};
    const auto& eye_color_info{RawData::RandomMiiEyeColor.at(static_cast<std::size_t>(race))};
    const auto& eyebrow_type_info{RawData::RandomMiiEyebrowType.at(index)};
    const auto& nose_type_info{RawData::RandomMiiNoseType.at(index)};
    const auto& mouth_type_info{RawData::RandomMiiMouthType.at(index)};
    const auto& glasses_type_info{RawData::RandomMiiGlassType.at(static_cast<std::size_t>(age))};

    data.faceline_type.Assign(
        faceline_type_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_type_info.values_count)]);
    data.faceline_color.Assign(
        faceline_color_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_color_info.values_count)]);
    data.faceline_wrinkle.Assign(
        faceline_wrinkle_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_wrinkle_info.values_count)]);
    data.faceline_makeup.Assign(
        faceline_makeup_info
            .values[MiiUtil::GetRandomValue<std::size_t>(faceline_makeup_info.values_count)]);

    data.hair_type.Assign(
        hair_type_info.values[MiiUtil::GetRandomValue<std::size_t>(hair_type_info.values_count)]);
    data.hair_color.Assign(RawData::GetHairColorFromVer3(
        hair_color_info
            .values[MiiUtil::GetRandomValue<std::size_t>(hair_color_info.values_count)]));
    data.hair_flip.Assign(MiiUtil::GetRandomValue<HairFlip>(HairFlip::Maximum));

    data.eye_type.Assign(
        eye_type_info.values[MiiUtil::GetRandomValue<std::size_t>(eye_type_info.values_count)]);

    const auto eye_rotate_1{gender != Gender::Male ? 4 : 2};
    const auto eye_rotate_2{gender != Gender::Male ? 3 : 4};
    const auto eye_rotate_offset{32 - RawData::EyeRotateLookup[eye_rotate_1] + eye_rotate_2};
    const auto eye_rotate{32 - RawData::EyeRotateLookup[data.eye_type]};

    data.eye_color.Assign(RawData::GetEyeColorFromVer3(
        eye_color_info.values[MiiUtil::GetRandomValue<std::size_t>(eye_color_info.values_count)]));
    data.eye_scale.Assign(4);
    data.eye_aspect.Assign(3);
    data.eye_rotate.Assign(eye_rotate_offset - eye_rotate);
    data.eye_x.Assign(2);
    data.eye_y.Assign(axis_y + 12);

    data.eyebrow_type.Assign(
        eyebrow_type_info
            .values[MiiUtil::GetRandomValue<std::size_t>(eyebrow_type_info.values_count)]);

    const auto eyebrow_rotate_1{race == Race::Asian ? 6 : 0};
    const auto eyebrow_y{race == Race::Asian ? 9 : 10};
    const auto eyebrow_rotate_offset{32 - RawData::EyebrowRotateLookup[eyebrow_rotate_1] + 6};
    const auto eyebrow_rotate{
        32 - RawData::EyebrowRotateLookup[static_cast<std::size_t>(data.eyebrow_type.Value())]};

    data.eyebrow_color.Assign(data.hair_color);
    data.eyebrow_scale.Assign(4);
    data.eyebrow_aspect.Assign(3);
    data.eyebrow_rotate.Assign(eyebrow_rotate_offset - eyebrow_rotate);
    data.eyebrow_x.Assign(2);
    data.eyebrow_y.Assign(axis_y + eyebrow_y);

    const auto nose_scale{gender == Gender::Female ? 3 : 4};

    data.nose_type.Assign(
        nose_type_info.values[MiiUtil::GetRandomValue<std::size_t>(nose_type_info.values_count)]);
    data.nose_scale.Assign(nose_scale);
    data.nose_y.Assign(axis_y + 9);

    const auto mouth_color{gender == Gender::Female ? MiiUtil::GetRandomValue<int>(4) : 0};

    data.mouth_type.Assign(
        mouth_type_info.values[MiiUtil::GetRandomValue<std::size_t>(mouth_type_info.values_count)]);
    data.mouth_color.Assign(RawData::GetMouthColorFromVer3(mouth_color));
    data.mouth_scale.Assign(4);
    data.mouth_aspect.Assign(3);
    data.mouth_y.Assign(axis_y + 13);

    data.beard_color.Assign(data.hair_color);
    data.mustache_scale.Assign(4);

    if (gender == Gender::Male && age != Age::Young && MiiUtil::GetRandomValue<int>(10) < 2) {
        const auto mustache_and_beard_flag{
            MiiUtil::GetRandomValue<BeardAndMustacheFlag>(BeardAndMustacheFlag::All)};

        auto beard_type{BeardType::None};
        auto mustache_type{MustacheType::None};

        if ((mustache_and_beard_flag & BeardAndMustacheFlag::Beard) ==
            BeardAndMustacheFlag::Beard) {
            beard_type = MiiUtil::GetRandomValue<BeardType>(BeardType::Beard1, BeardType::Beard5);
        }

        if ((mustache_and_beard_flag & BeardAndMustacheFlag::Mustache) ==
            BeardAndMustacheFlag::Mustache) {
            mustache_type = MiiUtil::GetRandomValue<MustacheType>(MustacheType::Mustache1,
                                                                  MustacheType::Mustache5);
        }

        data.mustache_type.Assign(mustache_type);
        data.beard_type.Assign(beard_type);
        data.mustache_y.Assign(10);
    } else {
        data.mustache_type.Assign(MustacheType::None);
        data.beard_type.Assign(BeardType::None);
        data.mustache_y.Assign(axis_y + 10);
    }

    const auto glasses_type_start{MiiUtil::GetRandomValue<std::size_t>(100)};
    u8 glasses_type{};
    while (glasses_type_start < glasses_type_info.values[glasses_type]) {
        if (++glasses_type >= glasses_type_info.values_count) {
            ASSERT(false);
            break;
        }
    }

    data.glasses_type.Assign(glasses_type);
    data.glasses_color.Assign(RawData::GetGlassColorFromVer3(0));
    data.glasses_scale.Assign(4);
    data.glasses_y.Assign(axis_y + 10);

    data.mole_type.Assign(0);
    data.mole_scale.Assign(4);
    data.mole_x.Assign(2);
    data.mole_y.Assign(20);
}

u32 CoreData::IsValid() const {
    // TODO: Complete this
    return 0;
}

void CoreData::SetFontRegion(FontRegion value) {
    data.font_region.Assign(value);
}

void CoreData::SetFavoriteColor(u8 value) {
    data.favorite_color.Assign(value);
}

void CoreData::SetGender(Gender value) {
    data.gender.Assign(value);
}

void CoreData::SetHeight(u8 value) {
    data.height.Assign(value);
}

void CoreData::SetBuild(u8 value) {
    data.build.Assign(value);
}

void CoreData::SetType(u8 value) {
    data.type.Assign(value);
}

void CoreData::SetRegionMove(u8 value) {
    data.region_move.Assign(value);
}

void CoreData::SetFacelineType(u8 value) {
    data.faceline_type.Assign(value);
}

void CoreData::SetFacelineColor(u8 value) {
    data.faceline_color.Assign(value);
}

void CoreData::SetFacelineWrinkle(u8 value) {
    data.faceline_wrinkle.Assign(value);
}

void CoreData::SetFacelineMake(u8 value) {
    data.faceline_makeup.Assign(value);
}

void CoreData::SetHairType(u8 value) {
    data.hair_type.Assign(value);
}

void CoreData::SetHairColor(u8 value) {
    data.hair_color.Assign(value);
}

void CoreData::SetHairFlip(HairFlip value) {
    data.hair_flip.Assign(value);
}

void CoreData::SetEyeType(u8 value) {
    data.eye_type.Assign(value);
}

void CoreData::SetEyeColor(u8 value) {
    data.eye_color.Assign(value);
}

void CoreData::SetEyeScale(u8 value) {
    data.eye_scale.Assign(value);
}

void CoreData::SetEyeAspect(u8 value) {
    data.eye_aspect.Assign(value);
}

void CoreData::SetEyeRotate(u8 value) {
    data.eye_rotate.Assign(value);
}

void CoreData::SetEyeX(u8 value) {
    data.eye_x.Assign(value);
}

void CoreData::SetEyeY(u8 value) {
    data.eye_y.Assign(value);
}

void CoreData::SetEyebrowType(u8 value) {
    data.eyebrow_type.Assign(value);
}

void CoreData::SetEyebrowColor(u8 value) {
    data.eyebrow_color.Assign(value);
}

void CoreData::SetEyebrowScale(u8 value) {
    data.eyebrow_scale.Assign(value);
}

void CoreData::SetEyebrowAspect(u8 value) {
    data.eyebrow_aspect.Assign(value);
}

void CoreData::SetEyebrowRotate(u8 value) {
    data.eyebrow_rotate.Assign(value);
}

void CoreData::SetEyebrowX(u8 value) {
    data.eyebrow_x.Assign(value);
}

void CoreData::SetEyebrowY(u8 value) {
    data.eyebrow_y.Assign(value);
}

void CoreData::SetNoseType(u8 value) {
    data.nose_type.Assign(value);
}

void CoreData::SetNoseScale(u8 value) {
    data.nose_scale.Assign(value);
}

void CoreData::SetNoseY(u8 value) {
    data.nose_y.Assign(value);
}

void CoreData::SetMouthType(u8 value) {
    data.mouth_type.Assign(value);
}

void CoreData::SetMouthColor(u8 value) {
    data.mouth_color.Assign(value);
}

void CoreData::SetMouthScale(u8 value) {
    data.mouth_scale.Assign(value);
}

void CoreData::SetMouthAspect(u8 value) {
    data.mouth_aspect.Assign(value);
}

void CoreData::SetMouthY(u8 value) {
    data.mouth_y.Assign(value);
}

void CoreData::SetBeardColor(u8 value) {
    data.beard_color.Assign(value);
}

void CoreData::SetBeardType(BeardType value) {
    data.beard_type.Assign(value);
}

void CoreData::SetMustacheType(MustacheType value) {
    data.mustache_type.Assign(value);
}

void CoreData::SetMustacheScale(u8 value) {
    data.mustache_scale.Assign(value);
}

void CoreData::SetMustacheY(u8 value) {
    data.mustache_y.Assign(value);
}

void CoreData::SetGlassType(u8 value) {
    data.glasses_type.Assign(value);
}

void CoreData::SetGlassColor(u8 value) {
    data.glasses_color.Assign(value);
}

void CoreData::SetGlassScale(u8 value) {
    data.glasses_scale.Assign(value);
}

void CoreData::SetGlassY(u8 value) {
    data.glasses_y.Assign(value);
}

void CoreData::SetMoleType(u8 value) {
    data.mole_type.Assign(value);
}

void CoreData::SetMoleScale(u8 value) {
    data.mole_scale.Assign(value);
}

void CoreData::SetMoleX(u8 value) {
    data.mole_x.Assign(value);
}

void CoreData::SetMoleY(u8 value) {
    data.mole_y.Assign(value);
}

void CoreData::SetNickname(Nickname nickname) {
    name = nickname;
}

u8 CoreData::GetFontRegion() const {
    return static_cast<u8>(data.font_region.Value());
}

u8 CoreData::GetFavoriteColor() const {
    return static_cast<u8>(data.favorite_color.Value());
}

u8 CoreData::GetGender() const {
    return static_cast<u8>(data.gender.Value());
}

u8 CoreData::GetHeight() const {
    return static_cast<u8>(data.height.Value());
}

u8 CoreData::GetBuild() const {
    return static_cast<u8>(data.build.Value());
}

u8 CoreData::GetType() const {
    return static_cast<u8>(data.type.Value());
}

u8 CoreData::GetRegionMove() const {
    return static_cast<u8>(data.region_move.Value());
}

u8 CoreData::GetFacelineType() const {
    return static_cast<u8>(data.faceline_type.Value());
}

u8 CoreData::GetFacelineColor() const {
    return static_cast<u8>(data.faceline_color.Value());
}

u8 CoreData::GetFacelineWrinkle() const {
    return static_cast<u8>(data.faceline_wrinkle.Value());
}

u8 CoreData::GetFacelineMake() const {
    return static_cast<u8>(data.faceline_makeup.Value());
}

u8 CoreData::GetHairType() const {
    return static_cast<u8>(data.hair_type.Value());
}

u8 CoreData::GetHairColor() const {
    return static_cast<u8>(data.hair_color.Value());
}

u8 CoreData::GetHairFlip() const {
    return static_cast<u8>(data.hair_flip.Value());
}

u8 CoreData::GetEyeType() const {
    return static_cast<u8>(data.eye_type.Value());
}

u8 CoreData::GetEyeColor() const {
    return static_cast<u8>(data.eye_color.Value());
}

u8 CoreData::GetEyeScale() const {
    return static_cast<u8>(data.eye_scale.Value());
}

u8 CoreData::GetEyeAspect() const {
    return static_cast<u8>(data.eye_aspect.Value());
}

u8 CoreData::GetEyeRotate() const {
    return static_cast<u8>(data.eye_rotate.Value());
}

u8 CoreData::GetEyeX() const {
    return static_cast<u8>(data.eye_x.Value());
}

u8 CoreData::GetEyeY() const {
    return static_cast<u8>(data.eye_y.Value());
}

u8 CoreData::GetEyebrowType() const {
    return static_cast<u8>(data.eyebrow_type.Value());
}

u8 CoreData::GetEyebrowColor() const {
    return static_cast<u8>(data.eyebrow_color.Value());
}

u8 CoreData::GetEyebrowScale() const {
    return static_cast<u8>(data.eyebrow_scale.Value());
}

u8 CoreData::GetEyebrowAspect() const {
    return static_cast<u8>(data.eyebrow_aspect.Value());
}

u8 CoreData::GetEyebrowRotate() const {
    return static_cast<u8>(data.eyebrow_rotate.Value());
}

u8 CoreData::GetEyebrowX() const {
    return static_cast<u8>(data.eyebrow_x.Value());
}

u8 CoreData::GetEyebrowY() const {
    return static_cast<u8>(data.eyebrow_y.Value());
}

u8 CoreData::GetNoseType() const {
    return static_cast<u8>(data.nose_type.Value());
}

u8 CoreData::GetNoseScale() const {
    return static_cast<u8>(data.nose_scale.Value());
}

u8 CoreData::GetNoseY() const {
    return static_cast<u8>(data.nose_y.Value());
}

u8 CoreData::GetMouthType() const {
    return static_cast<u8>(data.mouth_type.Value());
}

u8 CoreData::GetMouthColor() const {
    return static_cast<u8>(data.mouth_color.Value());
}

u8 CoreData::GetMouthScale() const {
    return static_cast<u8>(data.mouth_scale.Value());
}

u8 CoreData::GetMouthAspect() const {
    return static_cast<u8>(data.mouth_aspect.Value());
}

u8 CoreData::GetMouthY() const {
    return static_cast<u8>(data.mouth_y.Value());
}

u8 CoreData::GetBeardColor() const {
    return static_cast<u8>(data.beard_color.Value());
}

u8 CoreData::GetBeardType() const {
    return static_cast<u8>(data.beard_type.Value());
}

u8 CoreData::GetMustacheType() const {
    return static_cast<u8>(data.mustache_type.Value());
}

u8 CoreData::GetMustacheScale() const {
    return static_cast<u8>(data.mustache_scale.Value());
}

u8 CoreData::GetMustacheY() const {
    return static_cast<u8>(data.mustache_y.Value());
}

u8 CoreData::GetGlassType() const {
    return static_cast<u8>(data.glasses_type.Value());
}

u8 CoreData::GetGlassColor() const {
    return static_cast<u8>(data.glasses_color.Value());
}

u8 CoreData::GetGlassScale() const {
    return static_cast<u8>(data.glasses_scale.Value());
}

u8 CoreData::GetGlassY() const {
    return static_cast<u8>(data.glasses_y.Value());
}

u8 CoreData::GetMoleType() const {
    return static_cast<u8>(data.mole_type.Value());
}

u8 CoreData::GetMoleScale() const {
    return static_cast<u8>(data.mole_scale.Value());
}

u8 CoreData::GetMoleX() const {
    return static_cast<u8>(data.mole_x.Value());
}

u8 CoreData::GetMoleY() const {
    return static_cast<u8>(data.mole_y.Value());
}

Nickname CoreData::GetNickname() const {
    return name;
}

Nickname CoreData::GetDefaultNickname() const {
    return {u'n', u'o', u' ', u'n', u'a', u'm', u'e'};
}

Nickname CoreData::GetInvalidNickname() const {
    return {u'?', u'?', u' ', u'?'};
}

} // namespace Service::Mii
