// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <chrono>
#include <ctime>

#include "core/hle/service/time/ephemeral_network_system_clock_context_writer.h"
#include "core/hle/service/time/local_system_clock_context_writer.h"
#include "core/hle/service/time/network_system_clock_context_writer.h"
#include "core/hle/service/time/time_manager.h"
#include "core/settings.h"

namespace Service::Time {

constexpr Clock::TimeSpanType standard_network_clock_accuracy{0x0009356907420000ULL};

static std::chrono::seconds GetSecondsSinceEpoch() {
    return std::chrono::duration_cast<std::chrono::seconds>(
               std::chrono::system_clock::now().time_since_epoch()) +
           Settings::values.custom_rtc_differential;
}

static s64 GetExternalRtcValue() {
    return GetSecondsSinceEpoch().count();
}

TimeManager::TimeManager(Core::System& system)
    : shared_memory{system}, standard_local_system_clock_core{standard_steady_clock_core},
      standard_network_system_clock_core{standard_steady_clock_core},
      standard_user_system_clock_core{standard_local_system_clock_core,
                                      standard_network_system_clock_core, system},
      ephemeral_network_system_clock_core{tick_based_steady_clock_core},
      local_system_clock_context_writer{
          std::make_shared<Clock::LocalSystemClockContextWriter>(shared_memory)},
      network_system_clock_context_writer{
          std::make_shared<Clock::NetworkSystemClockContextWriter>(shared_memory)},
      ephemeral_network_system_clock_context_writer{
          std::make_shared<Clock::EphemeralNetworkSystemClockContextWriter>()},
      time_zone_content_manager{*this, system} {

    const auto system_time{Clock::TimeSpanType::FromSeconds(GetExternalRtcValue())};
    SetupStandardSteadyClock(system, Common::UUID::Generate(), system_time, {}, {});
    SetupStandardLocalSystemClock(system, {}, system_time.ToSeconds());
    SetupStandardNetworkSystemClock({}, standard_network_clock_accuracy);
    SetupStandardUserSystemClock(system, {}, Clock::SteadyClockTimePoint::GetRandom());
    SetupEphemeralNetworkSystemClock();
}

TimeManager::~TimeManager() = default;

void TimeManager::SetupTimeZoneManager(std::string location_name,
                                       Clock::SteadyClockTimePoint time_zone_updated_time_point,
                                       std::size_t total_location_name_count,
                                       u128 time_zone_rule_version,
                                       FileSys::VirtualFile& vfs_file) {
    if (time_zone_content_manager.GetTimeZoneManager().SetDeviceLocationNameWithTimeZoneRule(
            location_name, vfs_file) != RESULT_SUCCESS) {
        UNREACHABLE();
        return;
    }

    time_zone_content_manager.GetTimeZoneManager().SetUpdatedTime(time_zone_updated_time_point);
    time_zone_content_manager.GetTimeZoneManager().SetTotalLocationNameCount(
        total_location_name_count);
    time_zone_content_manager.GetTimeZoneManager().SetTimeZoneRuleVersion(time_zone_rule_version);
    time_zone_content_manager.GetTimeZoneManager().MarkAsInitialized();
}

void TimeManager::SetupStandardSteadyClock(Core::System& system, Common::UUID clock_source_id,
                                           Clock::TimeSpanType setup_value,
                                           Clock::TimeSpanType internal_offset,
                                           bool is_rtc_reset_detected) {
    standard_steady_clock_core.SetClockSourceId(clock_source_id);
    standard_steady_clock_core.SetSetupValue(setup_value);
    standard_steady_clock_core.SetInternalOffset(internal_offset);
    standard_steady_clock_core.MarkAsInitialized();

    const auto current_time_point{standard_steady_clock_core.GetCurrentRawTimePoint(system)};
    shared_memory.SetupStandardSteadyClock(system, clock_source_id, current_time_point);
}

void TimeManager::SetupStandardLocalSystemClock(Core::System& system,
                                                Clock::SystemClockContext clock_context,
                                                s64 posix_time) {
    standard_local_system_clock_core.SetUpdateCallbackInstance(local_system_clock_context_writer);

    const auto current_time_point{
        standard_local_system_clock_core.GetSteadyClockCore().GetCurrentTimePoint(system)};
    if (current_time_point.clock_source_id == clock_context.steady_time_point.clock_source_id) {
        standard_local_system_clock_core.SetSystemClockContext(clock_context);
    } else {
        if (standard_local_system_clock_core.SetCurrentTime(system, posix_time) != RESULT_SUCCESS) {
            UNREACHABLE();
            return;
        }
    }

    standard_local_system_clock_core.MarkAsInitialized();
}

void TimeManager::SetupStandardNetworkSystemClock(Clock::SystemClockContext clock_context,
                                                  Clock::TimeSpanType sufficient_accuracy) {
    standard_network_system_clock_core.SetUpdateCallbackInstance(
        network_system_clock_context_writer);

    if (standard_network_system_clock_core.SetSystemClockContext(clock_context) != RESULT_SUCCESS) {
        UNREACHABLE();
        return;
    }

    standard_network_system_clock_core.SetStandardNetworkClockSufficientAccuracy(
        sufficient_accuracy);
    standard_network_system_clock_core.MarkAsInitialized();
}

void TimeManager::SetupStandardUserSystemClock(
    Core::System& system, bool is_automatic_correction_enabled,
    Clock::SteadyClockTimePoint steady_clock_time_point) {
    if (standard_user_system_clock_core.SetAutomaticCorrectionEnabled(
            system, is_automatic_correction_enabled) != RESULT_SUCCESS) {
        UNREACHABLE();
        return;
    }

    standard_user_system_clock_core.SetAutomaticCorrectionUpdatedTime(steady_clock_time_point);
    standard_user_system_clock_core.MarkAsInitialized();
    shared_memory.SetAutomaticCorrectionEnabled(is_automatic_correction_enabled);
}

void TimeManager::SetupEphemeralNetworkSystemClock() {
    ephemeral_network_system_clock_core.SetUpdateCallbackInstance(
        ephemeral_network_system_clock_context_writer);
    ephemeral_network_system_clock_core.MarkAsInitialized();
}

} // namespace Service::Time
