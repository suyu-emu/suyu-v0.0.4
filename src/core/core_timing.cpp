// SPDX-FileCopyrightText: Copyright 2020 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <algorithm>
#include <mutex>
#include <string>
#include <tuple>

#ifdef _WIN32
#include "common/windows/timer_resolution.h"
#endif

#ifdef ARCHITECTURE_x86_64
#include "common/x64/cpu_wait.h"
#endif

#include "common/microprofile.h"
#include "core/core_timing.h"
#include "core/hardware_properties.h"

namespace Core::Timing {

constexpr s64 MAX_SLICE_LENGTH = 10000;

std::shared_ptr<EventType> CreateEvent(std::string name, TimedCallback&& callback) {
    return std::make_shared<EventType>(std::move(callback), std::move(name));
}

CoreTiming::CoreTiming() : clock{Common::CreateOptimalClock()} {}

CoreTiming::~CoreTiming() {
    Reset();
}

void CoreTiming::ThreadEntry(CoreTiming& instance) {
    static constexpr char name[] = "HostTiming";
    MicroProfileOnThreadCreate(name);
    Common::SetCurrentThreadName(name);
    Common::SetCurrentThreadPriority(Common::ThreadPriority::High);
    instance.on_thread_init();
    instance.ThreadLoop();
    MicroProfileOnThreadExit();
}

void CoreTiming::Initialize(std::function<void()>&& on_thread_init_) {
    Reset();
    on_thread_init = std::move(on_thread_init_);
    event_fifo_id = 0;
    shutting_down = false;
    cpu_ticks = 0;
    if (is_multicore) {
        timer_thread = std::make_unique<std::jthread>(ThreadEntry, std::ref(*this));
    }
}

void CoreTiming::ClearPendingEvents() {
    std::scoped_lock lock{advance_lock, basic_lock};
    event_queue.clear();
    event.Set();
}

void CoreTiming::Pause(bool is_paused) {
    paused = is_paused;
    pause_event.Set();

    if (!is_paused) {
        pause_end_time = GetGlobalTimeNs().count();
    }
}

void CoreTiming::SyncPause(bool is_paused) {
    if (is_paused == paused && paused_set == is_paused) {
        return;
    }

    Pause(is_paused);
    if (timer_thread) {
        if (!is_paused) {
            pause_event.Set();
        }
        event.Set();
        while (paused_set != is_paused)
            ;
    }

    if (!is_paused) {
        pause_end_time = GetGlobalTimeNs().count();
    }
}

bool CoreTiming::IsRunning() const {
    return !paused_set;
}

bool CoreTiming::HasPendingEvents() const {
    std::scoped_lock lock{basic_lock};
    return !event_queue.empty();
}

void CoreTiming::ScheduleEvent(std::chrono::nanoseconds ns_into_future,
                               const std::shared_ptr<EventType>& event_type, bool absolute_time) {
    {
        std::scoped_lock scope{basic_lock};
        const auto next_time{absolute_time ? ns_into_future : GetGlobalTimeNs() + ns_into_future};

        event_queue.emplace_back(Event{next_time.count(), event_fifo_id++, event_type});
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }

    event.Set();
}

void CoreTiming::ScheduleLoopingEvent(std::chrono::nanoseconds start_time,
                                      std::chrono::nanoseconds resched_time,
                                      const std::shared_ptr<EventType>& event_type,
                                      bool absolute_time) {
    {
        std::scoped_lock scope{basic_lock};
        const auto next_time{absolute_time ? start_time : GetGlobalTimeNs() + start_time};

        event_queue.emplace_back(
            Event{next_time.count(), event_fifo_id++, event_type, resched_time.count()});
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }

    event.Set();
}

void CoreTiming::UnscheduleEvent(const std::shared_ptr<EventType>& event_type,
                                 UnscheduleEventType type) {
    {
        std::scoped_lock lk{basic_lock};

        event_queue.erase(
            std::remove_if(event_queue.begin(), event_queue.end(),
                           [&](const Event& e) { return e.type.lock().get() == event_type.get(); }),
            event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());

        event_type->sequence_number++;
    }

    // Force any in-progress events to finish
    if (type == UnscheduleEventType::Wait) {
        std::scoped_lock lk{advance_lock};
    }
}

void CoreTiming::AddTicks(u64 ticks_to_add) {
    cpu_ticks += ticks_to_add;
    downcount -= static_cast<s64>(ticks_to_add);
}

void CoreTiming::Idle() {
    cpu_ticks += 1000U;
}

void CoreTiming::ResetTicks() {
    downcount.store(MAX_SLICE_LENGTH, std::memory_order_release);
}

u64 CoreTiming::GetClockTicks() const {
    if (is_multicore) [[likely]] {
        return clock->GetCNTPCT();
    }
    return Common::WallClock::CPUTickToCNTPCT(cpu_ticks);
}

u64 CoreTiming::GetGPUTicks() const {
    if (is_multicore) [[likely]] {
        return clock->GetGPUTick();
    }
    return Common::WallClock::CPUTickToGPUTick(cpu_ticks);
}

std::optional<s64> CoreTiming::Advance() {
    std::scoped_lock lock{advance_lock, basic_lock};
    global_timer = GetGlobalTimeNs().count();

    while (!event_queue.empty() && event_queue.front().time <= global_timer) {
        Event evt = std::move(event_queue.front());
        std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
        event_queue.pop_back();

        if (const auto event_type = evt.type.lock()) {
            const auto evt_time = evt.time;
            const auto evt_sequence_num = event_type->sequence_number;

            basic_lock.unlock();

            const auto new_schedule_time = event_type->callback(
                evt_time, std::chrono::nanoseconds{GetGlobalTimeNs().count() - evt_time});

            basic_lock.lock();

            if (evt_sequence_num != event_type->sequence_number) {
                continue;
            }

            if (new_schedule_time.has_value() || evt.reschedule_time != 0) {
                const auto next_schedule_time = new_schedule_time.value_or(
                    std::chrono::nanoseconds{evt.reschedule_time});

                auto next_time = evt.time + next_schedule_time.count();
                if (evt.time < pause_end_time) {
                    next_time = pause_end_time + next_schedule_time.count();
                }

                event_queue.emplace_back(Event{next_time, event_fifo_id++, evt.type,
                                               next_schedule_time.count()});
                std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
            }
        }

        global_timer = GetGlobalTimeNs().count();
    }

    if (!event_queue.empty()) {
        return event_queue.front().time;
    } else {
        return std::nullopt;
    }
}

void CoreTiming::ThreadLoop() {
    has_started = true;
    while (!shutting_down) {
        while (!paused) {
            paused_set = false;
            const auto next_time = Advance();
            if (next_time) {
                // There are more events left in the queue, wait until the next event.
                auto wait_time = *next_time - GetGlobalTimeNs().count();
                if (wait_time > 0) {
#ifdef _WIN32
                    while (!paused && !event.IsSet() && wait_time > 0) {
                        wait_time = *next_time - GetGlobalTimeNs().count();
                        if (wait_time >= 1'000'000) { // 1ms
                            Common::Windows::SleepForOneTick();
                        } else {
#ifdef ARCHITECTURE_x86_64
                            Common::X64::MicroSleep();
#else
                            std::this_thread::yield();
#endif
                        }
                    }

                    if (event.IsSet()) {
                        event.Reset();
                    }
#else
                    event.WaitFor(std::chrono::nanoseconds(wait_time));
#endif
                }
            } else {
                // Queue is empty, wait until another event is scheduled and signals us to
                // continue.
                event.Wait();
            }
        }

        paused_set = true;
        pause_event.Wait();
    }
}

void CoreTiming::Reset() {
    paused = true;
    shutting_down = true;
    pause_event.Set();
    event.Set();
    if (timer_thread) {
        timer_thread->join();
    }
    timer_thread.reset();
    has_started = false;
}

std::chrono::nanoseconds CoreTiming::GetGlobalTimeNs() const {
    if (is_multicore) [[likely]] {
        return clock->GetTimeNS();
    }
    return std::chrono::nanoseconds{Common::WallClock::CPUTickToNS(cpu_ticks)};
}

std::chrono::microseconds CoreTiming::GetGlobalTimeUs() const {
    if (is_multicore) [[likely]] {
        return clock->GetTimeUS();
    }
    return std::chrono::microseconds{Common::WallClock::CPUTickToUS(cpu_ticks)};
}

} // namespace Core::Timing
