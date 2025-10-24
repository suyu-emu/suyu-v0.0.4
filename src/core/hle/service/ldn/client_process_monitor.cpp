// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/cmif_serialization.h"
#include "core/hle/service/ldn/ldn_results.h"
#include "core/hle/service/ldn/client_process_monitor.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::LDN {

IClientProcessMonitor::IClientProcessMonitor(Core::System& system_)
    : ServiceFramework{system_, "IClientProcessMonitor"} {
    static const FunctionInfo functions[] = {
        {0, D<&IClientProcessMonitor::RegisterClient>, "RegisterClient"},
    };
    RegisterHandlers(functions);
}

IClientProcessMonitor::~IClientProcessMonitor() = default;

Result IClientProcessMonitor::RegisterClient() {
    LOG_WARNING(Service_LDN, "(STUBBED) called");

    R_SUCCEED();
}

} // namespace Service::LDN
