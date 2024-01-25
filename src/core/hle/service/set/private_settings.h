// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/uuid.h"
#include "core/hle/service/psc/time/common.h"

namespace Service::Set {

/// This is nn::settings::system::InitialLaunchFlag
struct InitialLaunchFlag {
    union {
        u32 raw{};

        BitField<0, 1, u32> InitialLaunchCompletionFlag;
        BitField<8, 1, u32> InitialLaunchUserAdditionFlag;
        BitField<16, 1, u32> InitialLaunchTimestampFlag;
    };
};
static_assert(sizeof(InitialLaunchFlag) == 4, "InitialLaunchFlag is an invalid size");

/// This is nn::settings::system::InitialLaunchSettings
struct InitialLaunchSettings {
    InitialLaunchFlag flags;
    INSERT_PADDING_BYTES(0x4);
    Service::PSC::Time::SteadyClockTimePoint timestamp;
};
static_assert(sizeof(InitialLaunchSettings) == 0x20, "InitialLaunchSettings is incorrect size");

#pragma pack(push, 4)
struct InitialLaunchSettingsPacked {
    InitialLaunchFlag flags;
    Service::PSC::Time::SteadyClockTimePoint timestamp;
};
#pragma pack(pop)
static_assert(sizeof(InitialLaunchSettingsPacked) == 0x1C,
              "InitialLaunchSettingsPacked is incorrect size");

struct PrivateSettings {
    std::array<u8, 0x10> reserved_00;

    // nn::settings::system::InitialLaunchSettings
    InitialLaunchSettings initial_launch_settings;

    std::array<u8, 0x20> reserved_30;

    Common::UUID external_clock_source_id;
    s64 shutdown_rtc_value;
    s64 external_steady_clock_internal_offset;

    std::array<u8, 0x60> reserved_70;

    // nn::settings::system::PlatformRegion
    std::array<u8, 0x4> platform_region;

    std::array<u8, 0x4> reserved_D4;
};
static_assert(offsetof(PrivateSettings, initial_launch_settings) == 0x10);
static_assert(offsetof(PrivateSettings, external_clock_source_id) == 0x50);
static_assert(offsetof(PrivateSettings, reserved_70) == 0x70);
static_assert(offsetof(PrivateSettings, platform_region) == 0xD0);
static_assert(sizeof(PrivateSettings) == 0xD8, "PrivateSettings has the wrong size!");

PrivateSettings DefaultPrivateSettings();

} // namespace Service::Set
