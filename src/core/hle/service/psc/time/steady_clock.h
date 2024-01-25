// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {

class SteadyClock final : public ServiceFramework<SteadyClock> {
public:
    explicit SteadyClock(Core::System& system, std::shared_ptr<TimeManager> manager,
                         bool can_write_steady_clock, bool can_write_uninitialized_clock);

    ~SteadyClock() override = default;

    Result GetCurrentTimePoint(SteadyClockTimePoint& out_time_point);
    Result GetTestOffset(s64& out_test_offset);
    Result SetTestOffset(s64 test_offset);
    Result GetRtcValue(s64& out_rtc_value);
    Result IsRtcResetDetected(bool& out_is_detected);
    Result GetSetupResultValue(Result& out_result);
    Result GetInternalOffset(s64& out_internal_offset);

private:
    void Handle_GetCurrentTimePoint(HLERequestContext& ctx);
    void Handle_GetTestOffset(HLERequestContext& ctx);
    void Handle_SetTestOffset(HLERequestContext& ctx);
    void Handle_GetRtcValue(HLERequestContext& ctx);
    void Handle_IsRtcResetDetected(HLERequestContext& ctx);
    void Handle_GetSetupResultValue(HLERequestContext& ctx);
    void Handle_GetInternalOffset(HLERequestContext& ctx);

    Core::System& m_system;

    StandardSteadyClockCore& m_clock_core;
    bool m_can_write_steady_clock;
    bool m_can_write_uninitialized_clock;
};

} // namespace Service::PSC::Time
