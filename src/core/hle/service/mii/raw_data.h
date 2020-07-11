// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "common/common_types.h"

namespace Service::Mii::RawData {

extern const std::array<u8, 1728> DefaultMii;
extern const std::array<u8, 3672> RandomMiiFaceline;
extern const std::array<u8, 1200> RandomMiiFacelineColor;
extern const std::array<u8, 3672> RandomMiiFacelineWrinkle;
extern const std::array<u8, 3672> RandomMiiFacelineMakeup;
extern const std::array<u8, 3672> RandomMiiHairType;
extern const std::array<u8, 1800> RandomMiiHairColor;
extern const std::array<u8, 3672> RandomMiiEyeType;
extern const std::array<u8, 588> RandomMiiEyeColor;
extern const std::array<u8, 3672> RandomMiiEyebrowType;
extern const std::array<u8, 3672> RandomMiiNoseType;
extern const std::array<u8, 3672> RandomMiiMouthType;
extern const std::array<u8, 588> RandomMiiGlassType;

} // namespace Service::Mii::RawData
