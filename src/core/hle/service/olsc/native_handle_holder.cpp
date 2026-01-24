// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/olsc/native_handle_holder.h"

#include "core/hle/kernel/k_event.h"
#include "core/hle/kernel/k_readable_event.h"

namespace Service::OLSC {

INativeHandleHolder::INativeHandleHolder(Core::System& system_)
    : ServiceFramework{system_, "INativeHandleHolder"}, service_context{system_, "OLSC"} {
    event = service_context.CreateEvent("OLSC::INativeHandleHolder");
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, D<&INativeHandleHolder::GetNativeHandle>, "GetNativeHandle"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

INativeHandleHolder::~INativeHandleHolder() {
    service_context.CloseEvent(event);
    event = nullptr;
}

Result INativeHandleHolder::GetNativeHandle(OutCopyHandle<Kernel::KReadableEvent> out_event) {
    LOG_WARNING(Service_OLSC, "(STUBBED) called");
    if (event) {
        event->Signal();
        *out_event = std::addressof(event->GetReadableEvent());
    } else {
        *out_event = nullptr;
    }
    R_SUCCEED();
}

} // namespace Service::OLSC
