// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <mutex>

#include "core/hle/kernel/k_event.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/psc/time/clocks/standard_steady_clock_core.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/power_state_request_manager.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::PSC::Time {
class TimeManager;

enum AlarmType : u32 {
    WakeupAlarm = 0,
    BackgroundTaskAlarm = 1,
};

struct Alarm : public Common::IntrusiveListBaseNode<Alarm> {
    using AlarmList = Common::IntrusiveListBaseTraits<Alarm>::ListType;

    Alarm(Core::System& system, KernelHelpers::ServiceContext& ctx, AlarmType type);
    ~Alarm();

    Kernel::KReadableEvent& GetEventHandle() {
        return m_event->GetReadableEvent();
    }

    s64 GetAlertTime() const {
        return m_alert_time;
    }

    void SetAlertTime(s64 time) {
        m_alert_time = time;
    }

    u32 GetPriority() const {
        return m_priority;
    }

    void Signal(Kernel::KernelCore& kernel) {
        m_event->Signal(kernel);
    }

    Result Lock() {
        // TODO
        // if (m_lock_service) {
        //     return m_lock_service->Lock();
        // }
        R_SUCCEED();
    }

    KernelHelpers::ServiceContext& m_ctx;

    u32 m_priority;
    Kernel::KEvent* m_event{};
    s64 m_alert_time{};
    // TODO
    // nn::psc::sf::IPmStateLock* m_lock_service{};
};

class Alarms {
public:
    explicit Alarms(Core::System& system, StandardSteadyClockCore& steady_clock, PowerStateRequestManager& power_state_request_manager);
    ~Alarms();

    Kernel::KEvent& GetEvent() {
        return *m_event;
    }

    s64 GetRawTime() {
        return m_steady_clock.GetRawTime();
    }

    Result Enable(Kernel::KernelCore& kernel, Alarm& alarm, s64 time);
    void Disable(Kernel::KernelCore& kernel, Alarm& alarm);
    void CheckAndSignal(Kernel::KernelCore& kernel);
    bool GetClosestAlarm(Alarm** out_alarm);

private:
    void Insert(Kernel::KernelCore& kernel, Alarm& alarm);
    void Erase(Kernel::KernelCore& kernel, Alarm& alarm);
    Result UpdateClosestAndSignal(Kernel::KernelCore& kernel);

    Core::System& m_system;
    KernelHelpers::ServiceContext m_ctx;

    StandardSteadyClockCore& m_steady_clock;
    PowerStateRequestManager& m_power_state_request_manager;
    Alarm::AlarmList m_alarms;
    Kernel::KEvent* m_event{};
    Alarm* m_closest_alarm{};
    std::mutex m_mutex;
};

class IAlarmService final : public ServiceFramework<IAlarmService> {
public:
    explicit IAlarmService(Core::System& system, std::shared_ptr<TimeManager> manager);
    ~IAlarmService() override = default;

private:
    void CreateWakeupAlarm(HLERequestContext& ctx);
    void CreateBackgroundTaskAlarm(HLERequestContext& ctx);
    Alarms& m_alarms;
};

class ISteadyClockAlarm final : public ServiceFramework<ISteadyClockAlarm> {
public:
    explicit ISteadyClockAlarm(Core::System& system, Alarms& alarms, AlarmType type);
    ~ISteadyClockAlarm() override = default;

private:
    void GetAlarmEvent(HLERequestContext& ctx);
    void Enable(HLERequestContext& ctx);
    void Disable(HLERequestContext& ctx);
    void IsEnabled(HLERequestContext& ctx);

    KernelHelpers::ServiceContext m_ctx;

    Alarms& m_alarms;
    Alarm m_alarm;
};

} // namespace Service::PSC::Time
