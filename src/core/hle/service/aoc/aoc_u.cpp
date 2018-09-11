// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/aoc/aoc_u.h"

namespace Service::AOC {

AOC_U::AOC_U() : ServiceFramework("aoc:u") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "CountAddOnContentByApplicationId"},
        {1, nullptr, "ListAddOnContentByApplicationId"},
        {2, &AOC_U::CountAddOnContent, "CountAddOnContent"},
        {3, &AOC_U::ListAddOnContent, "ListAddOnContent"},
        {4, nullptr, "GetAddOnContentBaseIdByApplicationId"},
        {5, nullptr, "GetAddOnContentBaseId"},
        {6, nullptr, "PrepareAddOnContentByApplicationId"},
        {7, nullptr, "PrepareAddOnContent"},
        {8, nullptr, "GetAddOnContentListChangedEvent"},
    };
    RegisterHandlers(functions);
}

AOC_U::~AOC_U() = default;

void AOC_U::CountAddOnContent(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(0);
    LOG_WARNING(Service_AOC, "(STUBBED) called");
}

void AOC_U::ListAddOnContent(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(RESULT_SUCCESS);
    rb.Push<u64>(0);
    LOG_WARNING(Service_AOC, "(STUBBED) called");
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<AOC_U>()->InstallAsService(service_manager);
}

} // namespace Service::AOC
