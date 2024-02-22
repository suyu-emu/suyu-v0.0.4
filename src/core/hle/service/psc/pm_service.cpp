// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/psc/pm_module.h"
#include "core/hle/service/psc/pm_service.h"

namespace Service::PSC {

IPmService::IPmService(Core::System& system_) : ServiceFramework{system_, "psc:m"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IPmService::GetPmModule, "GetPmModule"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IPmService::~IPmService() = default;

void IPmService::GetPmModule(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PSC, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IPmModule>(system);
}

} // namespace Service::PSC
