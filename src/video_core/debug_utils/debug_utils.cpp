// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <mutex>

#include "video_core/debug_utils/debug_utils.h"

namespace Tegra {

void DebugContext::DoOnEvent(Event event, void* data) {
    {
        std::unique_lock<std::mutex> lock(breakpoint_mutex);

        // TODO(Subv): Commit the rasterizer's caches so framebuffers, render targets, etc. will
        // show on debug widgets

        // TODO: Should stop the CPU thread here once we multithread emulation.

        active_breakpoint = event;
        at_breakpoint = true;

        // Tell all observers that we hit a breakpoint
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnMaxwellBreakPointHit(event, data);
        }

        // Wait until another thread tells us to Resume()
        resume_from_breakpoint.wait(lock, [&] { return !at_breakpoint; });
    }
}

void DebugContext::Resume() {
    {
        std::lock_guard<std::mutex> lock(breakpoint_mutex);

        // Tell all observers that we are about to resume
        for (auto& breakpoint_observer : breakpoint_observers) {
            breakpoint_observer->OnMaxwellResume();
        }

        // Resume the waiting thread (i.e. OnEvent())
        at_breakpoint = false;
    }

    resume_from_breakpoint.notify_one();
}

} // namespace Tegra
