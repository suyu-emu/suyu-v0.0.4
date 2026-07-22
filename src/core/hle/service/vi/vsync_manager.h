// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <set>

namespace Kernel {
class KernelCore;
}

namespace Service {
class Event;
}

namespace Service::VI {

class DisplayList;

class VsyncManager {
public:
    explicit VsyncManager();
    ~VsyncManager();

    void SignalVsync(Kernel::KernelCore& kernel);
    void LinkVsyncEvent(Event* event) {
        m_vsync_events.insert(event);
    }
    void UnlinkVsyncEvent(Event* event) {
        m_vsync_events.erase(event);
    }

private:
    std::set<Event*> m_vsync_events;
};

} // namespace Service::VI
