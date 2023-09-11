// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/mii/mii_util.h"
#include "core/hle/service/mii/types/char_info.h"
#include "core/hle/service/mii/types/raw_data.h"
#include "core/hle/service/mii/types/store_data.h"
#include "core/hle/service/mii/types/ver3_store_data.h"

namespace Service::Mii {

void Ver3StoreData::BuildToStoreData(CharInfo& out_char_info) const {
    if (!IsValid()) {
        return;
    }

    // TODO: We are ignoring a bunch of data from the mii_v3

    out_char_info.gender = static_cast<u8>(mii_information.gender);
    out_char_info.favorite_color = static_cast<u8>(mii_information.favorite_color);
    out_char_info.height = height;
    out_char_info.build = build;

    // Copy name until string terminator
    out_char_info.name = {};
    for (std::size_t index = 0; index < out_char_info.name.size() - 1; index++) {
        out_char_info.name[index] = mii_name[index];
        if (out_char_info.name[index] == 0) {
            break;
        }
    }

    out_char_info.font_region = region_information.character_set;

    out_char_info.faceline_type = appearance_bits1.face_shape;
    out_char_info.faceline_color = appearance_bits1.skin_color;
    out_char_info.faceline_wrinkle = appearance_bits2.wrinkles;
    out_char_info.faceline_make = appearance_bits2.makeup;

    out_char_info.hair_type = hair_style;
    out_char_info.hair_color = appearance_bits3.hair_color;
    out_char_info.hair_flip = appearance_bits3.flip_hair;

    out_char_info.eye_type = static_cast<u8>(appearance_bits4.eye_type);
    out_char_info.eye_color = static_cast<u8>(appearance_bits4.eye_color);
    out_char_info.eye_scale = static_cast<u8>(appearance_bits4.eye_scale);
    out_char_info.eye_aspect = static_cast<u8>(appearance_bits4.eye_vertical_stretch);
    out_char_info.eye_rotate = static_cast<u8>(appearance_bits4.eye_rotation);
    out_char_info.eye_x = static_cast<u8>(appearance_bits4.eye_spacing);
    out_char_info.eye_y = static_cast<u8>(appearance_bits4.eye_y_position);

    out_char_info.eyebrow_type = static_cast<u8>(appearance_bits5.eyebrow_style);
    out_char_info.eyebrow_color = static_cast<u8>(appearance_bits5.eyebrow_color);
    out_char_info.eyebrow_scale = static_cast<u8>(appearance_bits5.eyebrow_scale);
    out_char_info.eyebrow_aspect = static_cast<u8>(appearance_bits5.eyebrow_yscale);
    out_char_info.eyebrow_rotate = static_cast<u8>(appearance_bits5.eyebrow_rotation);
    out_char_info.eyebrow_x = static_cast<u8>(appearance_bits5.eyebrow_spacing);
    out_char_info.eyebrow_y = static_cast<u8>(appearance_bits5.eyebrow_y_position);

    out_char_info.nose_type = static_cast<u8>(appearance_bits6.nose_type);
    out_char_info.nose_scale = static_cast<u8>(appearance_bits6.nose_scale);
    out_char_info.nose_y = static_cast<u8>(appearance_bits6.nose_y_position);

    out_char_info.mouth_type = static_cast<u8>(appearance_bits7.mouth_type);
    out_char_info.mouth_color = static_cast<u8>(appearance_bits7.mouth_color);
    out_char_info.mouth_scale = static_cast<u8>(appearance_bits7.mouth_scale);
    out_char_info.mouth_aspect = static_cast<u8>(appearance_bits7.mouth_horizontal_stretch);
    out_char_info.mouth_y = static_cast<u8>(appearance_bits8.mouth_y_position);

    out_char_info.mustache_type = static_cast<u8>(appearance_bits8.mustache_type);
    out_char_info.mustache_scale = static_cast<u8>(appearance_bits9.mustache_scale);
    out_char_info.mustache_y = static_cast<u8>(appearance_bits9.mustache_y_position);

    out_char_info.beard_type = static_cast<u8>(appearance_bits9.bear_type);
    out_char_info.beard_color = static_cast<u8>(appearance_bits9.facial_hair_color);

    out_char_info.glasses_type = static_cast<u8>(appearance_bits10.glasses_type);
    out_char_info.glasses_color = static_cast<u8>(appearance_bits10.glasses_color);
    out_char_info.glasses_scale = static_cast<u8>(appearance_bits10.glasses_scale);
    out_char_info.glasses_y = static_cast<u8>(appearance_bits10.glasses_y_position);

    out_char_info.mole_type = static_cast<u8>(appearance_bits11.mole_enabled);
    out_char_info.mole_scale = static_cast<u8>(appearance_bits11.mole_scale);
    out_char_info.mole_x = static_cast<u8>(appearance_bits11.mole_x_position);
    out_char_info.mole_y = static_cast<u8>(appearance_bits11.mole_y_position);
}

void Ver3StoreData::BuildFromStoreData(const CharInfo& char_info) {
    version = 1;
    mii_information.gender.Assign(char_info.gender);
    mii_information.favorite_color.Assign(char_info.favorite_color);
    height = char_info.height;
    build = char_info.build;

    // Copy name until string terminator
    mii_name = {};
    for (std::size_t index = 0; index < char_info.name.size() - 1; index++) {
        mii_name[index] = char_info.name[index];
        if (mii_name[index] == 0) {
            break;
        }
    }

    region_information.character_set.Assign(char_info.font_region);

    appearance_bits1.face_shape.Assign(char_info.faceline_type);
    appearance_bits2.wrinkles.Assign(char_info.faceline_wrinkle);
    appearance_bits2.makeup.Assign(char_info.faceline_make);

    hair_style = char_info.hair_type;
    appearance_bits3.flip_hair.Assign(char_info.hair_flip);

    appearance_bits4.eye_type.Assign(char_info.eye_type);
    appearance_bits4.eye_scale.Assign(char_info.eye_scale);
    appearance_bits4.eye_vertical_stretch.Assign(char_info.eye_aspect);
    appearance_bits4.eye_rotation.Assign(char_info.eye_rotate);
    appearance_bits4.eye_spacing.Assign(char_info.eye_x);
    appearance_bits4.eye_y_position.Assign(char_info.eye_y);

    appearance_bits5.eyebrow_style.Assign(char_info.eyebrow_type);
    appearance_bits5.eyebrow_scale.Assign(char_info.eyebrow_scale);
    appearance_bits5.eyebrow_yscale.Assign(char_info.eyebrow_aspect);
    appearance_bits5.eyebrow_rotation.Assign(char_info.eyebrow_rotate);
    appearance_bits5.eyebrow_spacing.Assign(char_info.eyebrow_x);
    appearance_bits5.eyebrow_y_position.Assign(char_info.eyebrow_y);

    appearance_bits6.nose_type.Assign(char_info.nose_type);
    appearance_bits6.nose_scale.Assign(char_info.nose_scale);
    appearance_bits6.nose_y_position.Assign(char_info.nose_y);

    appearance_bits7.mouth_type.Assign(char_info.mouth_type);
    appearance_bits7.mouth_scale.Assign(char_info.mouth_scale);
    appearance_bits7.mouth_horizontal_stretch.Assign(char_info.mouth_aspect);
    appearance_bits8.mouth_y_position.Assign(char_info.mouth_y);

    appearance_bits8.mustache_type.Assign(char_info.mustache_type);
    appearance_bits9.mustache_scale.Assign(char_info.mustache_scale);
    appearance_bits9.mustache_y_position.Assign(char_info.mustache_y);

    appearance_bits9.bear_type.Assign(char_info.beard_type);

    appearance_bits10.glasses_scale.Assign(char_info.glasses_scale);
    appearance_bits10.glasses_y_position.Assign(char_info.glasses_y);

    appearance_bits11.mole_enabled.Assign(char_info.mole_type);
    appearance_bits11.mole_scale.Assign(char_info.mole_scale);
    appearance_bits11.mole_x_position.Assign(char_info.mole_x);
    appearance_bits11.mole_y_position.Assign(char_info.mole_y);

    // These types are converted to V3 from a table
    appearance_bits1.skin_color.Assign(RawData::FromVer3GetFacelineColor(char_info.faceline_color));
    appearance_bits3.hair_color.Assign(RawData::FromVer3GetHairColor(char_info.hair_color));
    appearance_bits4.eye_color.Assign(RawData::FromVer3GetEyeColor(char_info.eye_color));
    appearance_bits5.eyebrow_color.Assign(RawData::FromVer3GetHairColor(char_info.eyebrow_color));
    appearance_bits7.mouth_color.Assign(RawData::FromVer3GetMouthlineColor(char_info.mouth_color));
    appearance_bits9.facial_hair_color.Assign(RawData::FromVer3GetHairColor(char_info.beard_color));
    appearance_bits10.glasses_color.Assign(RawData::FromVer3GetGlassColor(char_info.glasses_color));
    appearance_bits10.glasses_type.Assign(RawData::FromVer3GetGlassType(char_info.glasses_type));

    crc = MiiUtil::CalculateCrc16(&version, sizeof(Ver3StoreData) - sizeof(u16));
}

u32 Ver3StoreData::IsValid() const {
    bool is_valid = version == 0 || version == 3;

    is_valid = is_valid && (mii_name[0] != 0);

    is_valid = is_valid && (mii_information.birth_month < 13);
    is_valid = is_valid && (mii_information.birth_day < 32);
    is_valid = is_valid && (mii_information.favorite_color < 12);
    is_valid = is_valid && (height < 128);
    is_valid = is_valid && (build < 128);

    is_valid = is_valid && (appearance_bits1.face_shape < 12);
    is_valid = is_valid && (appearance_bits1.skin_color < 7);
    is_valid = is_valid && (appearance_bits2.wrinkles < 12);
    is_valid = is_valid && (appearance_bits2.makeup < 12);

    is_valid = is_valid && (hair_style < 132);
    is_valid = is_valid && (appearance_bits3.hair_color < 8);

    is_valid = is_valid && (appearance_bits4.eye_type < 60);
    is_valid = is_valid && (appearance_bits4.eye_color < 6);
    is_valid = is_valid && (appearance_bits4.eye_scale < 8);
    is_valid = is_valid && (appearance_bits4.eye_vertical_stretch < 7);
    is_valid = is_valid && (appearance_bits4.eye_rotation < 8);
    is_valid = is_valid && (appearance_bits4.eye_spacing < 13);
    is_valid = is_valid && (appearance_bits4.eye_y_position < 19);

    is_valid = is_valid && (appearance_bits5.eyebrow_style < 25);
    is_valid = is_valid && (appearance_bits5.eyebrow_color < 8);
    is_valid = is_valid && (appearance_bits5.eyebrow_scale < 9);
    is_valid = is_valid && (appearance_bits5.eyebrow_yscale < 7);
    is_valid = is_valid && (appearance_bits5.eyebrow_rotation < 12);
    is_valid = is_valid && (appearance_bits5.eyebrow_spacing < 12);
    is_valid = is_valid && (appearance_bits5.eyebrow_y_position < 19);

    is_valid = is_valid && (appearance_bits6.nose_type < 18);
    is_valid = is_valid && (appearance_bits6.nose_scale < 9);
    is_valid = is_valid && (appearance_bits6.nose_y_position < 19);

    is_valid = is_valid && (appearance_bits7.mouth_type < 36);
    is_valid = is_valid && (appearance_bits7.mouth_color < 5);
    is_valid = is_valid && (appearance_bits7.mouth_scale < 9);
    is_valid = is_valid && (appearance_bits7.mouth_horizontal_stretch < 7);
    is_valid = is_valid && (appearance_bits8.mouth_y_position < 19);

    is_valid = is_valid && (appearance_bits8.mustache_type < 6);
    is_valid = is_valid && (appearance_bits9.mustache_scale < 7);
    is_valid = is_valid && (appearance_bits9.mustache_y_position < 17);

    is_valid = is_valid && (appearance_bits9.bear_type < 6);
    is_valid = is_valid && (appearance_bits9.facial_hair_color < 8);

    is_valid = is_valid && (appearance_bits10.glasses_type < 9);
    is_valid = is_valid && (appearance_bits10.glasses_color < 6);
    is_valid = is_valid && (appearance_bits10.glasses_scale < 8);
    is_valid = is_valid && (appearance_bits10.glasses_y_position < 21);

    is_valid = is_valid && (appearance_bits11.mole_enabled < 2);
    is_valid = is_valid && (appearance_bits11.mole_scale < 9);
    is_valid = is_valid && (appearance_bits11.mole_x_position < 17);
    is_valid = is_valid && (appearance_bits11.mole_y_position < 31);

    return is_valid;
}

} // namespace Service::Mii
