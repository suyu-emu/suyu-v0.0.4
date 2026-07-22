// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string_view>
#include <fmt/format.h>
#include "common/common_types.h"

// This file contains yuzu's HLE API version constants.

namespace HLE::ApiVersion {

// Horizon OS version constants.

constexpr u8 HOS_VERSION_MAJOR = 22;
constexpr u8 HOS_VERSION_MINOR = 5;
constexpr u8 HOS_VERSION_MICRO = 0;

// NintendoSDK version constants.
constexpr u8 SDK_REVISION_MAJOR = 1;
constexpr u8 SDK_REVISION_MINOR = 0;

constexpr char PLATFORM_STRING[0x20] = "NX";
constexpr char VERSION_HASH[0x40] = "ae93061abbc7791fcf8d2f7e7b7b2d62163af697";
constexpr char DISPLAY_VERSION[0x18] = "22.5.0";
constexpr char DISPLAY_TITLE[0x80] = "NintendoSDK Firmware for NX 22.5.0-1.0 ";
constexpr char VERSION_DIGEST[0x40] = "CusHY#00160500#WxlZDREksAFSi6bQJEV6nm6-cHLg2jikdYRVN3bwqyE=";

// Atmosphere version constants.
constexpr u8 ATMOSPHERE_RELEASE_VERSION_MAJOR = 1;
constexpr u8 ATMOSPHERE_RELEASE_VERSION_MINOR = 11;
constexpr u8 ATMOSPHERE_RELEASE_VERSION_MICRO = 2;

constexpr u32 AtmosphereTargetFirmwareWithRevision(u8 major, u8 minor, u8 micro, u8 rev) {
    return u32{major} << 24 | u32{minor} << 16 | u32{micro} << 8 | u32{rev};
}

constexpr u32 AtmosphereTargetFirmware(u8 major, u8 minor, u8 micro) {
    return AtmosphereTargetFirmwareWithRevision(major, minor, micro, 0);
}

constexpr u32 GetTargetFirmware() {
    return AtmosphereTargetFirmware(HOS_VERSION_MAJOR, HOS_VERSION_MINOR, HOS_VERSION_MICRO);
}

} // namespace HLE::ApiVersion
