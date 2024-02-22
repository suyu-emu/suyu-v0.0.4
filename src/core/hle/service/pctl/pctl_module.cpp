// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/hle/service/kernel_helpers.h"
#include "core/hle/service/pctl/parental_control_service.h"
#include "core/hle/service/pctl/pctl.h"
#include "core/hle/service/pctl/pctl_module.h"
#include "core/hle/service/server_manager.h"

namespace Service::PCTL {

void Module::Interface::CreateService(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    // TODO(ogniK): Get TID from process

    rb.PushIpcInterface<IParentalControlService>(system, capability);
}

void Module::Interface::CreateServiceWithoutInitialize(HLERequestContext& ctx) {
    LOG_DEBUG(Service_PCTL, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IParentalControlService>(system, capability);
}

Module::Interface::Interface(Core::System& system_, std::shared_ptr<Module> module_,
                             const char* name_, Capability capability_)
    : ServiceFramework{system_, name_}, module{std::move(module_)}, capability{capability_} {}

Module::Interface::~Interface() = default;

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    auto module = std::make_shared<Module>();
    server_manager->RegisterNamedService(
        "pctl", std::make_shared<PCTL>(system, module, "pctl",
                                       Capability::Application | Capability::SnsPost |
                                           Capability::Status | Capability::StereoVision));
    // TODO(ogniK): Implement remaining capabilities
    server_manager->RegisterNamedService(
        "pctl:a", std::make_shared<PCTL>(system, module, "pctl:a", Capability::None));
    server_manager->RegisterNamedService(
        "pctl:r", std::make_shared<PCTL>(system, module, "pctl:r", Capability::None));
    server_manager->RegisterNamedService(
        "pctl:s", std::make_shared<PCTL>(system, module, "pctl:s", Capability::None));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::PCTL
