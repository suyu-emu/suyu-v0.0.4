// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/scope_exit.h"
#include "core/core.h"
#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_process.h"
#include "core/hle/kernel/k_scoped_resource_reservation.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/svc.h"

namespace Kernel::Svc {

Result SignalEvent(Core::System& system, Handle event_handle) {
    LOG_DEBUG(Kernel_SVC, "called, event_handle=0x{:08X}", event_handle);

    // Get the current handle table.
    const KHandleTable& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();

    // Get the event.
    KScopedAutoObject event = handle_table.GetObject<KEvent>(event_handle);
    R_UNLESS(event.IsNotNull(), ResultInvalidHandle);

    return event->Signal();
}

Result SignalEvent32(Core::System& system, Handle event_handle) {
    return SignalEvent(system, event_handle);
}

Result ClearEvent(Core::System& system, Handle event_handle) {
    LOG_TRACE(Kernel_SVC, "called, event_handle=0x{:08X}", event_handle);

    // Get the current handle table.
    const auto& handle_table = system.Kernel().CurrentProcess()->GetHandleTable();

    // Try to clear the writable event.
    {
        KScopedAutoObject event = handle_table.GetObject<KEvent>(event_handle);
        if (event.IsNotNull()) {
            return event->Clear();
        }
    }

    // Try to clear the readable event.
    {
        KScopedAutoObject readable_event = handle_table.GetObject<KReadableEvent>(event_handle);
        if (readable_event.IsNotNull()) {
            return readable_event->Clear();
        }
    }

    LOG_ERROR(Kernel_SVC, "Event handle does not exist, event_handle=0x{:08X}", event_handle);

    return ResultInvalidHandle;
}

Result ClearEvent32(Core::System& system, Handle event_handle) {
    return ClearEvent(system, event_handle);
}

Result CreateEvent(Core::System& system, Handle* out_write, Handle* out_read) {
    LOG_DEBUG(Kernel_SVC, "called");

    // Get the kernel reference and handle table.
    auto& kernel = system.Kernel();
    auto& handle_table = kernel.CurrentProcess()->GetHandleTable();

    // Reserve a new event from the process resource limit
    KScopedResourceReservation event_reservation(kernel.CurrentProcess(),
                                                 LimitableResource::EventCountMax);
    R_UNLESS(event_reservation.Succeeded(), ResultLimitReached);

    // Create a new event.
    KEvent* event = KEvent::Create(kernel);
    R_UNLESS(event != nullptr, ResultOutOfResource);

    // Initialize the event.
    event->Initialize(kernel.CurrentProcess());

    // Commit the thread reservation.
    event_reservation.Commit();

    // Ensure that we clean up the event (and its only references are handle table) on function end.
    SCOPE_EXIT({
        event->GetReadableEvent().Close();
        event->Close();
    });

    // Register the event.
    KEvent::Register(kernel, event);

    // Add the event to the handle table.
    R_TRY(handle_table.Add(out_write, event));

    // Ensure that we maintaing a clean handle state on exit.
    auto handle_guard = SCOPE_GUARD({ handle_table.Remove(*out_write); });

    // Add the readable event to the handle table.
    R_TRY(handle_table.Add(out_read, std::addressof(event->GetReadableEvent())));

    // We succeeded.
    handle_guard.Cancel();
    return ResultSuccess;
}

Result CreateEvent32(Core::System& system, Handle* out_write, Handle* out_read) {
    return CreateEvent(system, out_write, out_read);
}

} // namespace Kernel::Svc
