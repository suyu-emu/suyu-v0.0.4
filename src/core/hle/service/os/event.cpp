// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/kernel/k_event.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/os/event.h"

namespace Service {

Event::Event(KernelHelpers::ServiceContext& ctx_)
    : ctx{ctx_}
{
    m_event = ctx.CreateEvent("Event");
}

Event::~Event() {
    m_event->GetReadableEvent().Close(ctx.kernel);
    m_event->Close(ctx.kernel);
}

void Event::Signal(Kernel::KernelCore& kernel) noexcept {
    m_event->Signal(kernel);
}

void Event::Clear(Kernel::KernelCore& kernel) noexcept {
    m_event->Clear(kernel);
}

Kernel::KReadableEvent* Event::GetHandle() noexcept {
    return std::addressof(m_event->GetReadableEvent());
}

} // namespace Service
