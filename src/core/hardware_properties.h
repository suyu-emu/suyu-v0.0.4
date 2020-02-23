// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <tuple>

#include "common/common_types.h"

namespace Core {

namespace Hardware {

// The below clock rate is based on Switch's clockspeed being widely known as 1.020GHz
// The exact value used is of course unverified.
constexpr u64 BASE_CLOCK_RATE = 1019215872; // Switch cpu frequency is 1020MHz un/docked
constexpr u64 CNTFREQ = 19200000;           // Switch's hardware clock speed
constexpr u32 NUM_CPU_CORES = 4;            // Number of CPU Cores

} // namespace Hardware

constexpr u32 INVALID_HOST_THREAD_ID = 0xFFFFFFFF;

struct EmuThreadHandle {
    u32 host_handle;
    u32 guest_handle;

    u64 GetRaw() const {
        return (static_cast<u64>(host_handle) << 32) | guest_handle;
    }

    bool operator==(const EmuThreadHandle& rhs) const {
        return std::tie(host_handle, guest_handle) == std::tie(rhs.host_handle, rhs.guest_handle);
    }

    bool operator!=(const EmuThreadHandle& rhs) const {
        return !operator==(rhs);
    }

    static constexpr EmuThreadHandle InvalidHandle() {
        constexpr u32 invalid_handle = 0xFFFFFFFF;
        return {invalid_handle, invalid_handle};
    }
};

} // namespace Core
