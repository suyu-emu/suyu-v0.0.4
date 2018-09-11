// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sockets/sfdnsres.h"

namespace Service::Sockets {

void SFDNSRES::GetAddrInfo(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};

    rb.Push(RESULT_SUCCESS);
}

SFDNSRES::SFDNSRES() : ServiceFramework("sfdnsres") {
    static const FunctionInfo functions[] = {
        {0, nullptr, "SetDnsAddressesPrivate"},
        {1, nullptr, "GetDnsAddressPrivate"},
        {2, nullptr, "GetHostByName"},
        {3, nullptr, "GetHostByAddr"},
        {4, nullptr, "GetHostStringError"},
        {5, nullptr, "GetGaiStringError"},
        {6, &SFDNSRES::GetAddrInfo, "GetAddrInfo"},
        {7, nullptr, "GetNameInfo"},
        {8, nullptr, "RequestCancelHandle"},
        {9, nullptr, "CancelSocketCall"},
        {11, nullptr, "ClearDnsIpServerAddressArray"},
    };
    RegisterHandlers(functions);
}

SFDNSRES::~SFDNSRES() = default;

} // namespace Service::Sockets
