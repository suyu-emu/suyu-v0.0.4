// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/fatal/fatal.h"
#include "core/hle/service/fatal/fatal_p.h"
#include "core/hle/service/fatal/fatal_u.h"

namespace Service::Fatal {

Module::Interface::Interface(std::shared_ptr<Module> module, const char* name)
    : ServiceFramework(name), module(std::move(module)) {}

Module::Interface::~Interface() = default;

void Module::Interface::ThrowFatalWithPolicy(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    u32 error_code = rp.Pop<u32>();
    LOG_WARNING(Service_Fatal, "(STUBBED) called, error_code=0x{:X}", error_code);
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void Module::Interface::ThrowFatalWithCpuContext(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service_Fatal, "(STUBBED) called");
    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto module = std::make_shared<Module>();
    std::make_shared<Fatal_P>(module)->InstallAsService(service_manager);
    std::make_shared<Fatal_U>(module)->InstallAsService(service_manager);
}

} // namespace Service::Fatal
