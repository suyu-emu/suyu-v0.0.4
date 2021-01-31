// Copyright 2019 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/service/time/standard_local_system_clock_core.h"
#include "core/hle/service/time/standard_network_system_clock_core.h"
#include "core/hle/service/time/standard_user_system_clock_core.h"

namespace Service::Time::Clock {

StandardUserSystemClockCore::StandardUserSystemClockCore(
    StandardLocalSystemClockCore& local_system_clock_core,
    StandardNetworkSystemClockCore& network_system_clock_core, Core::System& system)
    : SystemClockCore(local_system_clock_core.GetSteadyClockCore()),
      local_system_clock_core{local_system_clock_core},
      network_system_clock_core{network_system_clock_core}, auto_correction_enabled{},
      auto_correction_time{SteadyClockTimePoint::GetRandom()},
      auto_correction_event{Kernel::KEvent::Create(
          system.Kernel(), "StandardUserSystemClockCore:AutoCorrectionEvent")} {
    auto_correction_event->Initialize();
}

ResultCode StandardUserSystemClockCore::SetAutomaticCorrectionEnabled(Core::System& system,
                                                                      bool value) {
    if (const ResultCode result{ApplyAutomaticCorrection(system, value)};
        result != RESULT_SUCCESS) {
        return result;
    }

    auto_correction_enabled = value;

    return RESULT_SUCCESS;
}

ResultCode StandardUserSystemClockCore::GetClockContext(Core::System& system,
                                                        SystemClockContext& context) const {
    if (const ResultCode result{ApplyAutomaticCorrection(system, false)};
        result != RESULT_SUCCESS) {
        return result;
    }

    return local_system_clock_core.GetClockContext(system, context);
}

ResultCode StandardUserSystemClockCore::Flush(const SystemClockContext& context) {
    UNREACHABLE();
    return ERROR_NOT_IMPLEMENTED;
}

ResultCode StandardUserSystemClockCore::SetClockContext(const SystemClockContext& context) {
    UNREACHABLE();
    return ERROR_NOT_IMPLEMENTED;
}

ResultCode StandardUserSystemClockCore::ApplyAutomaticCorrection(Core::System& system,
                                                                 bool value) const {
    if (auto_correction_enabled == value) {
        return RESULT_SUCCESS;
    }

    if (!network_system_clock_core.IsClockSetup(system)) {
        return ERROR_UNINITIALIZED_CLOCK;
    }

    SystemClockContext context{};
    if (const ResultCode result{network_system_clock_core.GetClockContext(system, context)};
        result != RESULT_SUCCESS) {
        return result;
    }

    local_system_clock_core.SetClockContext(context);

    return RESULT_SUCCESS;
}

} // namespace Service::Time::Clock
