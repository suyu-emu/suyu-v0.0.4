// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <cstddef>

#include "common/common_types.h"

namespace Service::Set {
struct ApplnSettings {
    std::array<u8, 0x10> reserved_000;

    // nn::util::Uuid MiiAuthorId, copied from system settings 0x94B0
    std::array<u8, 0x10> mii_author_id;

    std::array<u8, 0x30> reserved_020;

    // nn::settings::system::ServiceDiscoveryControlSettings
    std::array<u8, 0x4> service_discovery_control_settings;

    std::array<u8, 0x20> reserved_054;

    bool in_repair_process_enable_flag;

    std::array<u8, 0x3> pad_075;
};
static_assert(offsetof(ApplnSettings, mii_author_id) == 0x10);
static_assert(offsetof(ApplnSettings, service_discovery_control_settings) == 0x50);
static_assert(offsetof(ApplnSettings, in_repair_process_enable_flag) == 0x74);
static_assert(sizeof(ApplnSettings) == 0x78, "ApplnSettings has the wrong size!");

ApplnSettings DefaultApplnSettings();

} // namespace Service::Set
