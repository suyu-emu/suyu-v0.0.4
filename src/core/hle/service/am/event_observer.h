// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/polyfill_thread.h"
#include "common/thread.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/os/event.h"
#include "core/hle/service/os/multi_wait.h"

namespace Core {
class System;
}

namespace Service::AM {

struct Applet;
class ProcessHolder;
class WindowSystem;

class EventObserver {
public:
    explicit EventObserver(Core::System& system, WindowSystem& window_system);
    ~EventObserver();

    void TrackAppletProcess(Applet& applet);
    void RequestUpdate();

private:
    void LinkDeferred();
    MultiWaitHolder* WaitSignaled(std::stop_token stop_token);
    void Process(MultiWaitHolder* holder);
    bool WaitAndProcessImpl();
    void LoopProcess();
    void OnWakeupEvent(MultiWaitHolder* holder);
    void OnProcessEvent(ProcessHolder* holder);
    void DestroyAppletProcessHolderLocked(ProcessHolder* holder);

    // System reference and context.
    Core::System& m_system;
    KernelHelpers::ServiceContext m_context;

    // Window manager.
    WindowSystem& m_window_system;

    // Guest event handle to wake up the event loop processor.
    Event m_wakeup_event;
    MultiWaitHolder m_wakeup_holder;

    // Mutex to protect remaining members.
    std::mutex m_lock{};

    // List of owned process holders.
    Common::IntrusiveListBaseTraits<ProcessHolder>::ListType m_process_holder_list;

    // Multi-wait objects for new tasks.
    MultiWait m_multi_wait;
    MultiWait m_deferred_wait_list;

    // Processing thread.
    std::jthread m_thread{};
};

} // namespace Service::AM
