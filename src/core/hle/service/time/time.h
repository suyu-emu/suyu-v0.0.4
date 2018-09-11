// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "core/hle/service/service.h"

namespace Service::Time {

struct LocationName {
    std::array<u8, 0x24> name;
};
static_assert(sizeof(LocationName) == 0x24, "LocationName is incorrect size");

struct CalendarTime {
    u16_le year;
    u8 month; // Starts at 1
    u8 day;   // Starts at 1
    u8 hour;
    u8 minute;
    u8 second;
    INSERT_PADDING_BYTES(1);
};
static_assert(sizeof(CalendarTime) == 0x8, "CalendarTime structure has incorrect size");

struct CalendarAdditionalInfo {
    u32_le day_of_week;
    u32_le day_of_year;
    std::array<u8, 8> name;
    INSERT_PADDING_BYTES(1);
    s32_le utc_offset;
};
static_assert(sizeof(CalendarAdditionalInfo) == 0x18,
              "CalendarAdditionalInfo structure has incorrect size");

// TODO(mailwl) RE this structure
struct TimeZoneRule {
    INSERT_PADDING_BYTES(0x4000);
};

struct SteadyClockTimePoint {
    u64_le value;
    INSERT_PADDING_WORDS(4);
};
static_assert(sizeof(SteadyClockTimePoint) == 0x18, "SteadyClockTimePoint is incorrect size");

struct SystemClockContext {
    u64_le offset;
    SteadyClockTimePoint time_point;
};
static_assert(sizeof(SystemClockContext) == 0x20,
              "SystemClockContext structure has incorrect size");

class Module final {
public:
    class Interface : public ServiceFramework<Interface> {
    public:
        explicit Interface(std::shared_ptr<Module> time, const char* name);
        ~Interface() override;

        void GetStandardUserSystemClock(Kernel::HLERequestContext& ctx);
        void GetStandardNetworkSystemClock(Kernel::HLERequestContext& ctx);
        void GetStandardSteadyClock(Kernel::HLERequestContext& ctx);
        void GetTimeZoneService(Kernel::HLERequestContext& ctx);
        void GetStandardLocalSystemClock(Kernel::HLERequestContext& ctx);

    protected:
        std::shared_ptr<Module> time;
    };
};

/// Registers all Time services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::Time
