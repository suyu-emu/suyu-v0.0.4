// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/host_timing.h"

#include <algorithm>
#include <mutex>
#include <string>
#include <tuple>

#include "common/assert.h"
#include "common/thread.h"
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

CoreTiming::CoreTiming() = default;
CoreTiming::~CoreTiming() = default;

void CoreTiming::ThreadEntry(CoreTiming& instance) {
    instance.Advance();
}

void CoreTiming::Initialize() {
    event_fifo_id = 0;
    const auto empty_timed_callback = [](u64, s64) {};
    ev_lost = CreateEvent("_lost_event", empty_timed_callback);
    start_time = std::chrono::system_clock::now();
    timer_thread = std::make_unique<std::thread>(ThreadEntry, std::ref(*this));
}

void CoreTiming::Shutdown() {
    std::unique_lock<std::mutex> guard(inner_mutex);
    shutting_down = true;
    if (!is_set) {
        is_set = true;
        condvar.notify_one();
    }
    inner_mutex.unlock();
    timer_thread->join();
    ClearPendingEvents();
}

void CoreTiming::ScheduleEvent(s64 ns_into_future, const std::shared_ptr<EventType>& event_type,
                               u64 userdata) {
    std::lock_guard guard{inner_mutex};
    const u64 timeout = static_cast<u64>(GetGlobalTimeNs().count() + ns_into_future);

    event_queue.emplace_back(Event{timeout, event_fifo_id++, userdata, event_type});

    std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    if (!is_set) {
        is_set = true;
        condvar.notify_one();
    }
}

void CoreTiming::UnscheduleEvent(const std::shared_ptr<EventType>& event_type, u64 userdata) {
    std::lock_guard guard{inner_mutex};

    const auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type.lock().get() == event_type.get() && e.userdata == userdata;
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

u64 CoreTiming::GetCPUTicks() const {
    std::chrono::nanoseconds time_now = GetGlobalTimeNs();
    return Core::Timing::nsToCycles(time_now);
}

u64 CoreTiming::GetClockTicks() const {
    std::chrono::nanoseconds time_now = GetGlobalTimeNs();
    return Core::Timing::nsToClockCycles(time_now);
}

void CoreTiming::ClearPendingEvents() {
    event_queue.clear();
}

void CoreTiming::RemoveEvent(const std::shared_ptr<EventType>& event_type) {
    std::lock_guard guard{inner_mutex};

    const auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type.lock().get() == event_type.get();
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void CoreTiming::Advance() {
    while (true) {
        std::unique_lock<std::mutex> guard(inner_mutex);

        global_timer = GetGlobalTimeNs().count();

        while (!event_queue.empty() && event_queue.front().time <= global_timer) {
            Event evt = std::move(event_queue.front());
            std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
            event_queue.pop_back();
            inner_mutex.unlock();

            if (auto event_type{evt.type.lock()}) {
                event_type->callback(evt.userdata, global_timer - evt.time);
            }

            inner_mutex.lock();
        }
        auto next_time = std::chrono::nanoseconds(event_queue.front().time - global_timer);
        condvar.wait_for(guard, next_time, [this] { return is_set; });
        is_set = false;
        if (shutting_down) {
            break;
        }
    }
}

std::chrono::nanoseconds CoreTiming::GetGlobalTimeNs() const {
    sys_time_point current = std::chrono::system_clock::now();
    auto elapsed = current - start_time;
    return std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed);
}

std::chrono::microseconds CoreTiming::GetGlobalTimeUs() const {
    sys_time_point current = std::chrono::system_clock::now();
    auto elapsed = current - start_time;
    return std::chrono::duration_cast<std::chrono::microseconds>(elapsed);
}

} // namespace Core::Timing
