// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/os/event.h"
#include "core/hle/service/vi/vsync_manager.h"

namespace Service::VI {

VsyncManager::VsyncManager() = default;
VsyncManager::~VsyncManager() = default;

void VsyncManager::SignalVsync(Kernel::KernelCore& kernel) {
    for (auto* event : m_vsync_events) {
        event->Signal(kernel);
    }
}

} // namespace Service::VI
