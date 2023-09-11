// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/raw_data.h"
#include "core/hle/service/mii/types/store_data.h"
#include "core/hle/service/mii/types/ver3_store_data.h"

namespace Service::Mii {

void NfpStoreDataExtension::SetFromStoreData(const StoreData& store_data) {
    faceline_color = static_cast<u8>(store_data.GetFacelineColor() & 0xf);
    hair_color = static_cast<u8>(store_data.GetHairColor() & 0x7f);
    eye_color = static_cast<u8>(store_data.GetEyeColor() & 0x7f);
    eyebrow_color = static_cast<u8>(store_data.GetEyebrowColor() & 0x7f);
    mouth_color = static_cast<u8>(store_data.GetMouthColor() & 0x7f);
    beard_color = static_cast<u8>(store_data.GetBeardColor() & 0x7f);
    glass_color = static_cast<u8>(store_data.GetGlassColor() & 0x7f);
    glass_type = static_cast<u8>(store_data.GetGlassType() & 0x1f);
}

void Ver3StoreData::BuildToStoreData(StoreData& out_store_data) const {
    out_store_data.BuildBase(Gender::Male);

    if (!IsValid()) {
        return;
    }

    // TODO: We are ignoring a bunch of data from the mii_v3

    out_store_data.SetGender(static_cast<Gender>(static_cast<u8>(mii_information.gender)));
    out_store_data.SetFavoriteColor(static_cast<u8>(mii_information.favorite_color));
    out_store_data.SetHeight(height);
    out_store_data.SetBuild(build);

    out_store_data.SetNickname(mii_name);
    out_store_data.SetFontRegion(
        static_cast<FontRegion>(static_cast<u8>(region_information.font_region)));

    out_store_data.SetFacelineType(appearance_bits1.faceline_type);
    out_store_data.SetFacelineColor(appearance_bits1.faceline_color);
    out_store_data.SetFacelineWrinkle(appearance_bits2.faceline_wrinkle);
    out_store_data.SetFacelineMake(appearance_bits2.faceline_make);

    out_store_data.SetHairType(hair_type);
    out_store_data.SetHairColor(appearance_bits3.hair_color);
    out_store_data.SetHairFlip(static_cast<HairFlip>(static_cast<u8>(appearance_bits3.hair_flip)));

    out_store_data.SetEyeType(static_cast<u8>(appearance_bits4.eye_type));
    out_store_data.SetEyeColor(static_cast<u8>(appearance_bits4.eye_color));
    out_store_data.SetEyeScale(static_cast<u8>(appearance_bits4.eye_scale));
    out_store_data.SetEyeAspect(static_cast<u8>(appearance_bits4.eye_aspect));
    out_store_data.SetEyeRotate(static_cast<u8>(appearance_bits4.eye_rotate));
    out_store_data.SetEyeX(static_cast<u8>(appearance_bits4.eye_x));
    out_store_data.SetEyeY(static_cast<u8>(appearance_bits4.eye_y));

    out_store_data.SetEyebrowType(static_cast<u8>(appearance_bits5.eyebrow_type));
    out_store_data.SetEyebrowColor(static_cast<u8>(appearance_bits5.eyebrow_color));
    out_store_data.SetEyebrowScale(static_cast<u8>(appearance_bits5.eyebrow_scale));
    out_store_data.SetEyebrowAspect(static_cast<u8>(appearance_bits5.eyebrow_aspect));
    out_store_data.SetEyebrowRotate(static_cast<u8>(appearance_bits5.eyebrow_rotate));
    out_store_data.SetEyebrowX(static_cast<u8>(appearance_bits5.eyebrow_x));
    out_store_data.SetEyebrowY(static_cast<u8>(appearance_bits5.eyebrow_y));

    out_store_data.SetNoseType(static_cast<u8>(appearance_bits6.nose_type));
    out_store_data.SetNoseScale(static_cast<u8>(appearance_bits6.nose_scale));
    out_store_data.SetNoseY(static_cast<u8>(appearance_bits6.nose_y));

    out_store_data.SetMouthType(static_cast<u8>(appearance_bits7.mouth_type));
    out_store_data.SetMouthColor(static_cast<u8>(appearance_bits7.mouth_color));
    out_store_data.SetMouthScale(static_cast<u8>(appearance_bits7.mouth_scale));
    out_store_data.SetMouthAspect(static_cast<u8>(appearance_bits7.mouth_aspect));
    out_store_data.SetMouthY(static_cast<u8>(appearance_bits8.mouth_y));

    out_store_data.SetMustacheType(
        static_cast<MustacheType>(static_cast<u8>(appearance_bits8.mustache_type)));
    out_store_data.SetMustacheScale(static_cast<u8>(appearance_bits9.mustache_scale));
    out_store_data.SetMustacheY(static_cast<u8>(appearance_bits9.mustache_y));

    out_store_data.SetBeardType(
        static_cast<BeardType>(static_cast<u8>(appearance_bits9.beard_type)));
    out_store_data.SetBeardColor(static_cast<u8>(appearance_bits9.beard_color));

    out_store_data.SetGlassType(static_cast<u8>(appearance_bits10.glass_type));
    out_store_data.SetGlassColor(static_cast<u8>(appearance_bits10.glass_color));
    out_store_data.SetGlassScale(static_cast<u8>(appearance_bits10.glass_scale));
    out_store_data.SetGlassY(static_cast<u8>(appearance_bits10.glass_y));

    out_store_data.SetMoleType(static_cast<u8>(appearance_bits11.mole_type));
    out_store_data.SetMoleScale(static_cast<u8>(appearance_bits11.mole_scale));
    out_store_data.SetMoleX(static_cast<u8>(appearance_bits11.mole_x));
    out_store_data.SetMoleY(static_cast<u8>(appearance_bits11.mole_y));
}

void Ver3StoreData::BuildFromStoreData(const StoreData& store_data) {
    version = 1;
    mii_information.gender.Assign(store_data.GetGender());
    mii_information.favorite_color.Assign(store_data.GetFavoriteColor());
    height = store_data.GetHeight();
    build = store_data.GetBuild();

    mii_name = store_data.GetNickname();
    region_information.font_region.Assign(static_cast<u8>(store_data.GetFontRegion()));

    appearance_bits1.faceline_type.Assign(store_data.GetFacelineType());
    appearance_bits2.faceline_wrinkle.Assign(store_data.GetFacelineWrinkle());
    appearance_bits2.faceline_make.Assign(store_data.GetFacelineMake());

    hair_type = store_data.GetHairType();
    appearance_bits3.hair_flip.Assign(store_data.GetHairFlip());

    appearance_bits4.eye_type.Assign(store_data.GetEyeType());
    appearance_bits4.eye_scale.Assign(store_data.GetEyeScale());
    appearance_bits4.eye_aspect.Assign(store_data.GetEyebrowAspect());
    appearance_bits4.eye_rotate.Assign(store_data.GetEyeRotate());
    appearance_bits4.eye_x.Assign(store_data.GetEyeX());
    appearance_bits4.eye_y.Assign(store_data.GetEyeY());

    appearance_bits5.eyebrow_type.Assign(store_data.GetEyebrowType());
    appearance_bits5.eyebrow_scale.Assign(store_data.GetEyebrowScale());
    appearance_bits5.eyebrow_aspect.Assign(store_data.GetEyebrowAspect());
    appearance_bits5.eyebrow_rotate.Assign(store_data.GetEyebrowRotate());
    appearance_bits5.eyebrow_x.Assign(store_data.GetEyebrowX());
    appearance_bits5.eyebrow_y.Assign(store_data.GetEyebrowY());

    appearance_bits6.nose_type.Assign(store_data.GetNoseType());
    appearance_bits6.nose_scale.Assign(store_data.GetNoseScale());
    appearance_bits6.nose_y.Assign(store_data.GetNoseY());

    appearance_bits7.mouth_type.Assign(store_data.GetMouthType());
    appearance_bits7.mouth_scale.Assign(store_data.GetMouthScale());
    appearance_bits7.mouth_aspect.Assign(store_data.GetMouthAspect());
    appearance_bits8.mouth_y.Assign(store_data.GetMouthY());

    appearance_bits8.mustache_type.Assign(store_data.GetMustacheType());
    appearance_bits9.mustache_scale.Assign(store_data.GetMustacheScale());
    appearance_bits9.mustache_y.Assign(store_data.GetMustacheY());

    appearance_bits9.beard_type.Assign(store_data.GetBeardType());

    appearance_bits10.glass_scale.Assign(store_data.GetGlassScale());
    appearance_bits10.glass_y.Assign(store_data.GetGlassY());

    appearance_bits11.mole_type.Assign(store_data.GetMoleType());
    appearance_bits11.mole_scale.Assign(store_data.GetMoleScale());
    appearance_bits11.mole_x.Assign(store_data.GetMoleX());
    appearance_bits11.mole_y.Assign(store_data.GetMoleY());

    // These types are converted to V3 from a table
    appearance_bits1.faceline_color.Assign(
        RawData::FromVer3GetFacelineColor(store_data.GetFacelineColor()));
    appearance_bits3.hair_color.Assign(RawData::FromVer3GetHairColor(store_data.GetHairColor()));
    appearance_bits4.eye_color.Assign(RawData::FromVer3GetEyeColor(store_data.GetEyeColor()));
    appearance_bits5.eyebrow_color.Assign(
        RawData::FromVer3GetHairColor(store_data.GetEyebrowColor()));
    appearance_bits7.mouth_color.Assign(
        RawData::FromVer3GetMouthlineColor(store_data.GetMouthColor()));
    appearance_bits9.beard_color.Assign(RawData::FromVer3GetHairColor(store_data.GetBeardColor()));
    appearance_bits10.glass_color.Assign(
        RawData::FromVer3GetGlassColor(store_data.GetGlassColor()));
    appearance_bits10.glass_type.Assign(RawData::FromVer3GetGlassType(store_data.GetGlassType()));

    crc = MiiUtil::CalculateCrc16(&version, sizeof(Ver3StoreData) - sizeof(u16));
}

u32 Ver3StoreData::IsValid() const {
    bool is_valid = version == 0 || version == 3;

    is_valid = is_valid && (mii_name.data[0] != 0);

    is_valid = is_valid && (mii_information.birth_month < 13);
    is_valid = is_valid && (mii_information.birth_day < 32);
    is_valid = is_valid && (mii_information.favorite_color < 12);
    is_valid = is_valid && (height < 128);
    is_valid = is_valid && (build < 128);

    is_valid = is_valid && (appearance_bits1.faceline_type < 12);
    is_valid = is_valid && (appearance_bits1.faceline_color < 7);
    is_valid = is_valid && (appearance_bits2.faceline_wrinkle < 12);
    is_valid = is_valid && (appearance_bits2.faceline_make < 12);

    is_valid = is_valid && (hair_type < 132);
    is_valid = is_valid && (appearance_bits3.hair_color < 8);

    is_valid = is_valid && (appearance_bits4.eye_type < 60);
    is_valid = is_valid && (appearance_bits4.eye_color < 6);
    is_valid = is_valid && (appearance_bits4.eye_scale < 8);
    is_valid = is_valid && (appearance_bits4.eye_aspect < 7);
    is_valid = is_valid && (appearance_bits4.eye_rotate < 8);
    is_valid = is_valid && (appearance_bits4.eye_x < 13);
    is_valid = is_valid && (appearance_bits4.eye_y < 19);

    is_valid = is_valid && (appearance_bits5.eyebrow_type < 25);
    is_valid = is_valid && (appearance_bits5.eyebrow_color < 8);
    is_valid = is_valid && (appearance_bits5.eyebrow_scale < 9);
    is_valid = is_valid && (appearance_bits5.eyebrow_aspect < 7);
    is_valid = is_valid && (appearance_bits5.eyebrow_rotate < 12);
    is_valid = is_valid && (appearance_bits5.eyebrow_x < 12);
    is_valid = is_valid && (appearance_bits5.eyebrow_y < 19);

    is_valid = is_valid && (appearance_bits6.nose_type < 18);
    is_valid = is_valid && (appearance_bits6.nose_scale < 9);
    is_valid = is_valid && (appearance_bits6.nose_y < 19);

    is_valid = is_valid && (appearance_bits7.mouth_type < 36);
    is_valid = is_valid && (appearance_bits7.mouth_color < 5);
    is_valid = is_valid && (appearance_bits7.mouth_scale < 9);
    is_valid = is_valid && (appearance_bits7.mouth_aspect < 7);
    is_valid = is_valid && (appearance_bits8.mouth_y < 19);

    is_valid = is_valid && (appearance_bits8.mustache_type < 6);
    is_valid = is_valid && (appearance_bits9.mustache_scale < 7);
    is_valid = is_valid && (appearance_bits9.mustache_y < 17);

    is_valid = is_valid && (appearance_bits9.beard_type < 6);
    is_valid = is_valid && (appearance_bits9.beard_color < 8);

    is_valid = is_valid && (appearance_bits10.glass_type < 9);
    is_valid = is_valid && (appearance_bits10.glass_color < 6);
    is_valid = is_valid && (appearance_bits10.glass_scale < 8);
    is_valid = is_valid && (appearance_bits10.glass_y < 21);

    is_valid = is_valid && (appearance_bits11.mole_type < 2);
    is_valid = is_valid && (appearance_bits11.mole_scale < 9);
    is_valid = is_valid && (appearance_bits11.mole_x < 17);
    is_valid = is_valid && (appearance_bits11.mole_y < 31);

    return is_valid;
}

} // namespace Service::Mii
