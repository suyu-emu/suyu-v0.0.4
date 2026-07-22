// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core.h"
#include "core/hle/service/psc/time/clocks/context_writers.h"

namespace Service::PSC::Time {

void ContextWriter::SignalAllNodes(Kernel::KernelCore& kernel) {
    std::scoped_lock l{m_mutex};
    for (auto& operation : m_operation_events) {
        operation.m_event->Signal(kernel);
    }
}

void ContextWriter::Link(OperationEvent& operation_event) {
    std::scoped_lock l{m_mutex};
    m_operation_events.push_back(operation_event);
}

LocalSystemClockContextWriter::LocalSystemClockContextWriter(Core::System& system,
                                                             SharedMemory& shared_memory)
    : m_system{system}, m_shared_memory{shared_memory} {}

Result LocalSystemClockContextWriter::Write(const SystemClockContext& context) {
    if (m_in_use) {
        R_SUCCEED_IF(context == m_context);
        m_context = context;
    } else {
        m_context = context;
        m_in_use = true;
    }

    m_shared_memory.SetLocalSystemContext(context);

    SignalAllNodes(m_system.Kernel());

    R_SUCCEED();
}

NetworkSystemClockContextWriter::NetworkSystemClockContextWriter(Core::System& system,
                                                                 SharedMemory& shared_memory,
                                                                 SystemClockCore& system_clock)
    : m_system{system}, m_shared_memory{shared_memory}, m_system_clock{system_clock} {}

Result NetworkSystemClockContextWriter::Write(const SystemClockContext& context) {
    s64 time{};
    [[maybe_unused]] auto res = m_system_clock.GetCurrentTime(&time);

    if (m_in_use) {
        R_SUCCEED_IF(context == m_context);
        m_context = context;
    } else {
        m_context = context;
        m_in_use = true;
    }

    m_shared_memory.SetNetworkSystemContext(context);

    SignalAllNodes(m_system.Kernel());

    R_SUCCEED();
}

EphemeralNetworkSystemClockContextWriter::EphemeralNetworkSystemClockContextWriter(
    Core::System& system)
    : m_system{system} {}

Result EphemeralNetworkSystemClockContextWriter::Write(const SystemClockContext& context) {
    if (m_in_use) {
        R_SUCCEED_IF(context == m_context);
        m_context = context;
    } else {
        m_context = context;
        m_in_use = true;
    }

    SignalAllNodes(m_system.Kernel());

    R_SUCCEED();
}

} // namespace Service::PSC::Time
