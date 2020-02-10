// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/host_timing.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <tuple>

#include "common/assert.h"
#include "core/core_timing_util.h"

namespace Core::HostTiming {

std::shared_ptr<EventType> CreateEvent(std::string name, TimedCallback&& callback) {
    return std::make_shared<EventType>(std::move(callback), std::move(name));
}

struct CoreTiming::Event {
    u64 time;
    u64 fifo_order;
    u64 userdata;
    std::weak_ptr<EventType> type;

    // Sort by time, unless the times are the same, in which case sort by
    // the order added to the queue
    friend bool operator>(const Event& left, const Event& right) {
        return std::tie(left.time, left.fifo_order) > std::tie(right.time, right.fifo_order);
    }

    friend bool operator<(const Event& left, const Event& right) {
        return std::tie(left.time, left.fifo_order) < std::tie(right.time, right.fifo_order);
    }
};

CoreTiming::CoreTiming() {
    clock = Common::CreateBestMatchingClock(Core::Timing::BASE_CLOCK_RATE, Core::Timing::CNTFREQ);
}

CoreTiming::~CoreTiming() = default;

void CoreTiming::ThreadEntry(CoreTiming& instance) {
    instance.Advance();
}

void CoreTiming::Initialize() {
    event_fifo_id = 0;
    const auto empty_timed_callback = [](u64, s64) {};
    ev_lost = CreateEvent("_lost_event", empty_timed_callback);
    timer_thread = std::make_unique<std::thread>(ThreadEntry, std::ref(*this));
}

void CoreTiming::Shutdown() {
    paused = true;
    shutting_down = true;
    event.Set();
    timer_thread->join();
    ClearPendingEvents();
    timer_thread.reset();
    has_started = false;
}

void CoreTiming::Pause(bool is_paused) {
    paused = is_paused;
}

void CoreTiming::SyncPause(bool is_paused) {
    if (is_paused == paused && paused_set == paused) {
        return;
    }
    Pause(is_paused);
    event.Set();
    while (paused_set != is_paused)
        ;
}

bool CoreTiming::IsRunning() {
    return !paused_set;
}

bool CoreTiming::HasPendingEvents() {
    return !(wait_set && event_queue.empty());
}

void CoreTiming::ScheduleEvent(s64 ns_into_future, const std::shared_ptr<EventType>& event_type,
                               u64 userdata) {
    basic_lock.lock();
    const u64 timeout = static_cast<u64>(GetGlobalTimeNs().count() + ns_into_future);

    event_queue.emplace_back(Event{timeout, event_fifo_id++, userdata, event_type});

    std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    basic_lock.unlock();
    event.Set();
}

void CoreTiming::UnscheduleEvent(const std::shared_ptr<EventType>& event_type, u64 userdata) {
    basic_lock.lock();
    const auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type.lock().get() == event_type.get() && e.userdata == userdata;
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
    basic_lock.unlock();
}

u64 CoreTiming::GetCPUTicks() const {
    return clock->GetCPUCycles();
}

u64 CoreTiming::GetClockTicks() const {
    return clock->GetClockCycles();
}

void CoreTiming::ClearPendingEvents() {
    event_queue.clear();
}

void CoreTiming::RemoveEvent(const std::shared_ptr<EventType>& event_type) {
    basic_lock.lock();

    const auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type.lock().get() == event_type.get();
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
    basic_lock.unlock();
}

void CoreTiming::Advance() {
    has_started = true;
    while (!shutting_down) {
        while (!paused) {
            paused_set = false;
            basic_lock.lock();
            global_timer = GetGlobalTimeNs().count();

            while (!event_queue.empty() && event_queue.front().time <= global_timer) {
                Event evt = std::move(event_queue.front());
                std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
                event_queue.pop_back();
                basic_lock.unlock();

                if (auto event_type{evt.type.lock()}) {
                    event_type->callback(evt.userdata, global_timer - evt.time);
                }

                basic_lock.lock();
            }

            if (!event_queue.empty()) {
                std::chrono::nanoseconds next_time =
                    std::chrono::nanoseconds(event_queue.front().time - global_timer);
                basic_lock.unlock();
                event.WaitFor(next_time);
            } else {
                basic_lock.unlock();
                wait_set = true;
                event.Wait();
            }

            wait_set = false;
        }
        paused_set = true;
    }
}

std::chrono::nanoseconds CoreTiming::GetGlobalTimeNs() const {
    return clock->GetTimeNS();
}

std::chrono::microseconds CoreTiming::GetGlobalTimeUs() const {
    return clock->GetTimeUS();
}

} // namespace Core::HostTiming
