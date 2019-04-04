// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sockets/sfdnsres.h"

namespace Service::Sockets {

void SFDNSRES::GetAddrInfo(Kernel::HLERequestContext& ctx) {
    struct Parameters {
        u8 use_nsd_resolve;
        u32 unknown;
        u64 process_id;
    };

    IPC::RequestParser rp{ctx};
    const auto parameters = rp.PopRaw<Parameters>();

    LOG_WARNING(Service,
                "(STUBBED) called. use_nsd_resolve={}, unknown=0x{:08X}, process_id=0x{:016X}",
                parameters.use_nsd_resolve, parameters.unknown, parameters.process_id);

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
