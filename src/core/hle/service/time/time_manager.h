// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/file_sys/vfs_types.h"
#include "core/hle/service/time/clock_types.h"
#include "core/hle/service/time/ephemeral_network_system_clock_core.h"
#include "core/hle/service/time/standard_local_system_clock_core.h"
#include "core/hle/service/time/standard_network_system_clock_core.h"
#include "core/hle/service/time/standard_steady_clock_core.h"
#include "core/hle/service/time/standard_user_system_clock_core.h"
#include "core/hle/service/time/tick_based_steady_clock_core.h"
#include "core/hle/service/time/time_sharedmemory.h"
#include "core/hle/service/time/time_zone_content_manager.h"

namespace Service::Time {

namespace Clock {
class EphemeralNetworkSystemClockContextWriter;
class LocalSystemClockContextWriter;
class NetworkSystemClockContextWriter;
} // namespace Clock

// Parts of this implementation were based on Ryujinx (https://github.com/Ryujinx/Ryujinx/pull/783).
// This code was released under public domain.

class TimeManager final {
public:
    explicit TimeManager(Core::System& system);
    ~TimeManager();

    Clock::StandardSteadyClockCore& GetStandardSteadyClockCore() {
        return standard_steady_clock_core;
    }

    const Clock::StandardSteadyClockCore& GetStandardSteadyClockCore() const {
        return standard_steady_clock_core;
    }

    Clock::StandardLocalSystemClockCore& GetStandardLocalSystemClockCore() {
        return standard_local_system_clock_core;
    }

    const Clock::StandardLocalSystemClockCore& GetStandardLocalSystemClockCore() const {
        return standard_local_system_clock_core;
    }

    Clock::StandardNetworkSystemClockCore& GetStandardNetworkSystemClockCore() {
        return standard_network_system_clock_core;
    }

    const Clock::StandardNetworkSystemClockCore& GetStandardNetworkSystemClockCore() const {
        return standard_network_system_clock_core;
    }

    Clock::StandardUserSystemClockCore& GetStandardUserSystemClockCore() {
        return standard_user_system_clock_core;
    }

    const Clock::StandardUserSystemClockCore& GetStandardUserSystemClockCore() const {
        return standard_user_system_clock_core;
    }

    TimeZone::TimeZoneContentManager& GetTimeZoneContentManager() {
        return time_zone_content_manager;
    }

    const TimeZone::TimeZoneContentManager& GetTimeZoneContentManager() const {
        return time_zone_content_manager;
    }

    SharedMemory& GetSharedMemory() {
        return shared_memory;
    }

    const SharedMemory& GetSharedMemory() const {
        return shared_memory;
    }

    void SetupTimeZoneManager(std::string location_name,
                              Clock::SteadyClockTimePoint time_zone_updated_time_point,
                              std::size_t total_location_name_count, u128 time_zone_rule_version,
                              FileSys::VirtualFile& vfs_file);

private:
    void SetupStandardSteadyClock(Core::System& system, Common::UUID clock_source_id,
                                  Clock::TimeSpanType setup_value,
                                  Clock::TimeSpanType internal_offset, bool is_rtc_reset_detected);
    void SetupStandardLocalSystemClock(Core::System& system,
                                       Clock::SystemClockContext clock_context, s64 posix_time);
    void SetupStandardNetworkSystemClock(Clock::SystemClockContext clock_context,
                                         Clock::TimeSpanType sufficient_accuracy);
    void SetupStandardUserSystemClock(Core::System& system, bool is_automatic_correction_enabled,
                                      Clock::SteadyClockTimePoint steady_clock_time_point);
    void SetupEphemeralNetworkSystemClock();

    SharedMemory shared_memory;

    Clock::StandardSteadyClockCore standard_steady_clock_core;
    Clock::TickBasedSteadyClockCore tick_based_steady_clock_core;
    Clock::StandardLocalSystemClockCore standard_local_system_clock_core;
    Clock::StandardNetworkSystemClockCore standard_network_system_clock_core;
    Clock::StandardUserSystemClockCore standard_user_system_clock_core;
    Clock::EphemeralNetworkSystemClockCore ephemeral_network_system_clock_core;

    std::shared_ptr<Clock::LocalSystemClockContextWriter> local_system_clock_context_writer;
    std::shared_ptr<Clock::NetworkSystemClockContextWriter> network_system_clock_context_writer;
    std::shared_ptr<Clock::EphemeralNetworkSystemClockContextWriter>
        ephemeral_network_system_clock_context_writer;

    TimeZone::TimeZoneContentManager time_zone_content_manager;
};

} // namespace Service::Time
