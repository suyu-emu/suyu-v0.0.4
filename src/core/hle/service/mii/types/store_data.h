// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/mii/mii_types.h"
#include "core/hle/service/mii/types/core_data.h"

namespace Service::Mii {

class StoreData {
public:
    // nn::mii::detail::StoreDataRaw::BuildDefault
    void BuildDefault(u32 mii_index);
    // nn::mii::detail::StoreDataRaw::BuildDefault

    void BuildBase(Gender gender);
    // nn::mii::detail::StoreDataRaw::BuildRandom
    void BuildRandom(Age age, Gender gender, Race race);

    void SetInvalidName();

    bool IsSpecial() const;

    u32 IsValid() const;

    Common::UUID GetCreateId() const;
    FontRegion GetFontRegion() const;
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

    bool operator==(const StoreData& data);

private:
    CoreData core_data{};
    Common::UUID create_id{};
    u16 data_crc{};
    u16 device_crc{};
};
static_assert(sizeof(StoreData) == 0x44, "StoreData has incorrect size.");

struct StoreDataElement {
    StoreData store_data{};
    Source source{};
};
static_assert(sizeof(StoreDataElement) == 0x48, "StoreDataElement has incorrect size.");

}; // namespace Service::Mii
