// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/service/glue/time/file_timestamp_worker.h"
#include "core/hle/service/glue/time/standard_steady_clock_resource.h"
#include "core/hle/service/glue/time/worker.h"
#include "core/hle/service/psc/time/common.h"
#include "core/hle/service/psc/time/service_manager.h"
#include "core/hle/service/psc/time/static.h"
#include "core/hle/service/psc/time/system_clock.h"
#include "core/hle/service/set/system_settings_server.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Glue::Time {
namespace {

bool g_ig_report_network_clock_context_set{};
Service::PSC::Time::SystemClockContext g_report_network_clock_context{};
bool g_ig_report_ephemeral_clock_context_set{};
Service::PSC::Time::SystemClockContext g_report_ephemeral_clock_context{};

template <typename T>
T GetSettingsItemValue(std::shared_ptr<Service::Set::ISystemSettingsServer>& set_sys,
                       const char* category, const char* name) {
    std::vector<u8> interval_buf;
    auto res = set_sys->GetSettingsItemValue(interval_buf, category, name);
    ASSERT(res == ResultSuccess);

    T v{};
    std::memcpy(&v, interval_buf.data(), sizeof(T));
    return v;
}

} // namespace

TimeWorker::TimeWorker(Core::System& system, StandardSteadyClockResource& steady_clock_resource,
                       FileTimestampWorker& file_timestamp_worker)
    : m_system{system}, m_ctx{m_system, "Glue:58"}, m_event{m_ctx.CreateEvent("Glue:58:Event")},
      m_steady_clock_resource{steady_clock_resource},
      m_file_timestamp_worker{file_timestamp_worker}, m_timer_steady_clock{m_ctx.CreateEvent(
                                                          "Glue:58:SteadyClockTimerEvent")},
      m_timer_file_system{m_ctx.CreateEvent("Glue:58:FileTimeTimerEvent")},
      m_alarm_worker{m_system, m_steady_clock_resource}, m_pm_state_change_handler{m_alarm_worker} {
    g_ig_report_network_clock_context_set = false;
    g_report_network_clock_context = {};
    g_ig_report_ephemeral_clock_context_set = false;
    g_report_ephemeral_clock_context = {};

    m_timer_steady_clock_timing_event = Core::Timing::CreateEvent(
        "Time::SteadyClockEvent",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            m_timer_steady_clock->Signal();
            return std::nullopt;
        });

    m_timer_file_system_timing_event = Core::Timing::CreateEvent(
        "Time::SteadyClockEvent",
        [this](s64 time,
               std::chrono::nanoseconds ns_late) -> std::optional<std::chrono::nanoseconds> {
            m_timer_file_system->Signal();
            return std::nullopt;
        });
}

TimeWorker::~TimeWorker() {
    m_local_clock_event->Signal();
    m_network_clock_event->Signal();
    m_ephemeral_clock_event->Signal();
    std::this_thread::sleep_for(std::chrono::milliseconds(16));

    m_thread.request_stop();
    m_event->Signal();
    m_thread.join();

    m_ctx.CloseEvent(m_event);
    m_system.CoreTiming().UnscheduleEvent(m_timer_steady_clock_timing_event);
    m_ctx.CloseEvent(m_timer_steady_clock);
    m_system.CoreTiming().UnscheduleEvent(m_timer_file_system_timing_event);
    m_ctx.CloseEvent(m_timer_file_system);
}

void TimeWorker::Initialize(std::shared_ptr<Service::PSC::Time::StaticService> time_sm,
                            std::shared_ptr<Service::Set::ISystemSettingsServer> set_sys) {
    m_set_sys = std::move(set_sys);
    m_time_m =
        m_system.ServiceManager().GetService<Service::PSC::Time::ServiceManager>("time:m", true);
    m_time_sm = std::move(time_sm);

    m_alarm_worker.Initialize(m_time_m);

    auto steady_clock_interval_m = GetSettingsItemValue<s32>(
        m_set_sys, "time", "standard_steady_clock_rtc_update_interval_minutes");

    auto one_minute_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::minutes(1)).count()};
    s64 steady_clock_interval_ns{steady_clock_interval_m * one_minute_ns};

    m_system.CoreTiming().ScheduleLoopingEvent(std::chrono::nanoseconds(0),
                                               std::chrono::nanoseconds(steady_clock_interval_ns),
                                               m_timer_steady_clock_timing_event);

    auto fs_notify_time_s =
        GetSettingsItemValue<s32>(m_set_sys, "time", "notify_time_to_fs_interval_seconds");
    auto one_second_ns{
        std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::seconds(1)).count()};
    s64 fs_notify_time_ns{fs_notify_time_s * one_second_ns};

    m_system.CoreTiming().ScheduleLoopingEvent(std::chrono::nanoseconds(0),
                                               std::chrono::nanoseconds(fs_notify_time_ns),
                                               m_timer_file_system_timing_event);

    auto res = m_time_sm->GetStandardLocalSystemClock(m_local_clock);
    ASSERT(res == ResultSuccess);
    res = m_time_m->GetStandardLocalClockOperationEvent(&m_local_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_sm->GetStandardNetworkSystemClock(m_network_clock);
    ASSERT(res == ResultSuccess);
    res = m_time_m->GetStandardNetworkClockOperationEventForServiceManager(&m_network_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_sm->GetEphemeralNetworkSystemClock(m_ephemeral_clock);
    ASSERT(res == ResultSuccess);
    res =
        m_time_m->GetEphemeralNetworkClockOperationEventForServiceManager(&m_ephemeral_clock_event);
    ASSERT(res == ResultSuccess);

    res = m_time_m->GetStandardUserSystemClockAutomaticCorrectionUpdatedEvent(
        &m_standard_user_auto_correct_clock_event);
    ASSERT(res == ResultSuccess);
}

void TimeWorker::StartThread() {
    m_thread = std::jthread(std::bind_front(&TimeWorker::ThreadFunc, this));
}

void TimeWorker::ThreadFunc(std::stop_token stop_token) {
    Common::SetCurrentThreadName("TimeWorker");
    Common::SetCurrentThreadPriority(Common::ThreadPriority::Low);

    enum class EventType {
        Exit = 0,
        IpmModuleService_GetEvent = 1,
        PowerStateChange = 2,
        SignalAlarms = 3,
        UpdateLocalSystemClock = 4,
        UpdateNetworkSystemClock = 5,
        UpdateEphemeralSystemClock = 6,
        UpdateSteadyClock = 7,
        UpdateFileTimestamp = 8,
        AutoCorrect = 9,
        Max = 10,
    };

    s32 num_objs{};
    std::array<Kernel::KSynchronizationObject*, static_cast<u32>(EventType::Max)> wait_objs{};
    std::array<EventType, static_cast<u32>(EventType::Max)> wait_indices{};

    const auto AddWaiter{
        [&](Kernel::KSynchronizationObject* synchronization_object, EventType type) {
            // Open a new reference to the object.
            synchronization_object->Open();

            // Insert into the list.
            wait_indices[num_objs] = type;
            wait_objs[num_objs++] = synchronization_object;
        }};

    while (!stop_token.stop_requested()) {
        SCOPE_EXIT({
            for (s32 i = 0; i < num_objs; i++) {
                wait_objs[i]->Close();
            }
        });

        num_objs = {};
        wait_objs = {};
        if (m_pm_state_change_handler.m_priority != 0) {
            AddWaiter(&m_event->GetReadableEvent(), EventType::Exit);
            // TODO
            // AddWaiter(gIPmModuleService::GetEvent(), 1);
            AddWaiter(&m_alarm_worker.GetEvent().GetReadableEvent(), EventType::PowerStateChange);
        } else {
            AddWaiter(&m_event->GetReadableEvent(), EventType::Exit);
            // TODO
            // AddWaiter(gIPmModuleService::GetEvent(), 1);
            AddWaiter(&m_alarm_worker.GetEvent().GetReadableEvent(), EventType::PowerStateChange);
            AddWaiter(&m_alarm_worker.GetTimerEvent().GetReadableEvent(), EventType::SignalAlarms);
            AddWaiter(&m_local_clock_event->GetReadableEvent(), EventType::UpdateLocalSystemClock);
            AddWaiter(&m_network_clock_event->GetReadableEvent(),
                      EventType::UpdateNetworkSystemClock);
            AddWaiter(&m_ephemeral_clock_event->GetReadableEvent(),
                      EventType::UpdateEphemeralSystemClock);
            AddWaiter(&m_timer_steady_clock->GetReadableEvent(), EventType::UpdateSteadyClock);
            AddWaiter(&m_timer_file_system->GetReadableEvent(), EventType::UpdateFileTimestamp);
            AddWaiter(&m_standard_user_auto_correct_clock_event->GetReadableEvent(),
                      EventType::AutoCorrect);
        }

        s32 out_index{-1};
        Kernel::KSynchronizationObject::Wait(m_system.Kernel(), &out_index, wait_objs.data(),
                                             num_objs, -1);
        ASSERT(out_index >= 0 && out_index < num_objs);

        if (stop_token.stop_requested()) {
            return;
        }

        switch (wait_indices[out_index]) {
        case EventType::Exit:
            return;

        case EventType::IpmModuleService_GetEvent:
            // TODO
            // IPmModuleService::GetEvent()
            // clear the event
            // Handle power state change event
            break;

        case EventType::PowerStateChange:
            m_alarm_worker.GetEvent().Clear();
            if (m_pm_state_change_handler.m_priority <= 1) {
                m_alarm_worker.OnPowerStateChanged();
            }
            break;

        case EventType::SignalAlarms:
            m_alarm_worker.GetTimerEvent().Clear();
            m_time_m->CheckAndSignalAlarms();
            break;

        case EventType::UpdateLocalSystemClock: {
            m_local_clock_event->Clear();

            Service::PSC::Time::SystemClockContext context{};
            auto res = m_local_clock->GetSystemClockContext(context);
            ASSERT(res == ResultSuccess);

            m_set_sys->SetUserSystemClockContext(context);

            m_file_timestamp_worker.SetFilesystemPosixTime();
        } break;

        case EventType::UpdateNetworkSystemClock: {
            m_network_clock_event->Clear();
            Service::PSC::Time::SystemClockContext context{};
            auto res = m_network_clock->GetSystemClockContext(context);
            ASSERT(res == ResultSuccess);
            m_set_sys->SetNetworkSystemClockContext(context);

            s64 time{};
            if (m_network_clock->GetCurrentTime(time) != ResultSuccess) {
                break;
            }

            [[maybe_unused]] auto offset_before{
                g_ig_report_network_clock_context_set ? g_report_network_clock_context.offset : 0};
            // TODO system report "standard_netclock_operation"
            //              "clock_time" = time
            //              "context_offset_before" = offset_before
            //              "context_offset_after"  = context.offset
            g_report_network_clock_context = context;
            if (!g_ig_report_network_clock_context_set) {
                g_ig_report_network_clock_context_set = true;
            }

            m_file_timestamp_worker.SetFilesystemPosixTime();
        } break;

        case EventType::UpdateEphemeralSystemClock: {
            m_ephemeral_clock_event->Clear();

            Service::PSC::Time::SystemClockContext context{};
            auto res = m_ephemeral_clock->GetSystemClockContext(context);
            if (res != ResultSuccess) {
                break;
            }

            s64 time{};
            res = m_ephemeral_clock->GetCurrentTime(time);
            if (res != ResultSuccess) {
                break;
            }

            [[maybe_unused]] auto offset_before{g_ig_report_ephemeral_clock_context_set
                                                    ? g_report_ephemeral_clock_context.offset
                                                    : 0};
            // TODO system report "ephemeral_netclock_operation"
            //              "clock_time" = time
            //              "context_offset_before" = offset_before
            //              "context_offset_after"  = context.offset
            g_report_ephemeral_clock_context = context;
            if (!g_ig_report_ephemeral_clock_context_set) {
                g_ig_report_ephemeral_clock_context_set = true;
            }
        } break;

        case EventType::UpdateSteadyClock:
            m_timer_steady_clock->Clear();

            m_steady_clock_resource.UpdateTime();
            m_time_m->SetStandardSteadyClockBaseTime(m_steady_clock_resource.GetTime());
            break;

        case EventType::UpdateFileTimestamp:
            m_timer_file_system->Clear();

            m_file_timestamp_worker.SetFilesystemPosixTime();
            break;

        case EventType::AutoCorrect: {
            m_standard_user_auto_correct_clock_event->Clear();

            bool automatic_correction{};
            auto res = m_time_sm->IsStandardUserSystemClockAutomaticCorrectionEnabled(
                automatic_correction);
            ASSERT(res == ResultSuccess);

            Service::PSC::Time::SteadyClockTimePoint time_point{};
            res = m_time_sm->GetStandardUserSystemClockAutomaticCorrectionUpdatedTime(time_point);
            ASSERT(res == ResultSuccess);

            m_set_sys->SetUserSystemClockAutomaticCorrectionEnabled(automatic_correction);
            m_set_sys->SetUserSystemClockAutomaticCorrectionUpdatedTime(time_point);
        } break;

        default:
            UNREACHABLE();
            break;
        }
    }
}

} // namespace Service::Glue::Time
