// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/pctl/parental_control_service.h"
#include "core/hle/service/pctl/parental_control_service_factory.h"

namespace Service::PCTL {

IParentalControlServiceFactory::IParentalControlServiceFactory(Core::System& system_,
                                                               const char* name_,
                                                               Capability capability_)
    : ServiceFramework{system_, name_}, capability{capability_} {}

IParentalControlServiceFactory::~IParentalControlServiceFactory() = default;

void IParentalControlServiceFactory::CreateService(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    // TODO(ogniK): Get TID from process

    rb.PushIpcInterface<IParentalControlService>(system, capability);
}

void IParentalControlServiceFactory::CreateServiceWithoutInitialize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IParentalControlService>(system, capability);
}

} // namespace Service::PCTL
