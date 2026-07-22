// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Kernel {
class KernelCore;
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service {

namespace KernelHelpers {
class ServiceContext;
}

class Event {
public:
    explicit Event(KernelHelpers::ServiceContext& ctx);
    ~Event();

    void Signal(Kernel::KernelCore& kernel) noexcept;
    void Clear(Kernel::KernelCore& kernel) noexcept;
    Kernel::KReadableEvent* GetHandle() noexcept;

private:
    KernelHelpers::ServiceContext& ctx;
    Kernel::KEvent* m_event;
};

} // namespace Service
