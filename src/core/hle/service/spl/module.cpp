// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <vector>
#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/spl/csrng.h"
#include "core/hle/service/spl/module.h"
#include "core/hle/service/spl/spl.h"

namespace Service::SPL {

Module::Interface::Interface(std::shared_ptr<Module> module, const char* name)
    : ServiceFramework(name), module(std::move(module)) {}

Module::Interface::~Interface() = default;

void Module::Interface::GetRandomBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    size_t size = ctx.GetWriteBufferSize();

    std::vector<u8> data(size);
    std::generate(data.begin(), data.end(), std::rand);

    ctx.WriteBuffer(data);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(RESULT_SUCCESS);
    LOG_DEBUG(Service_SPL, "called");
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto module = std::make_shared<Module>();
    std::make_shared<CSRNG>(module)->InstallAsService(service_manager);
    std::make_shared<SPL>(module)->InstallAsService(service_manager);
}

} // namespace Service::SPL
