// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/sockets/bsd.h"

namespace Service::Sockets {

void BSD::RegisterClient(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // bsd errno
}

void BSD::StartMonitoring(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};

    rb.Push(RESULT_SUCCESS);
}

void BSD::Socket(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    u32 domain = rp.Pop<u32>();
    u32 type = rp.Pop<u32>();
    u32 protocol = rp.Pop<u32>();

    LOG_WARNING(Service, "(STUBBED) called domain={} type={} protocol={}", domain, type, protocol);

    u32 fd = next_fd++;

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(fd);
    rb.Push<u32>(0); // bsd errno
}

void BSD::Connect(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::SendTo(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

void BSD::Close(Kernel::HLERequestContext& ctx) {
    LOG_WARNING(Service, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // ret
    rb.Push<u32>(0); // bsd errno
}

BSD::BSD(const char* name) : ServiceFramework(name) {
    static const FunctionInfo functions[] = {
        {0, &BSD::RegisterClient, "RegisterClient"},
        {1, &BSD::StartMonitoring, "StartMonitoring"},
        {2, &BSD::Socket, "Socket"},
        {3, nullptr, "SocketExempt"},
        {4, nullptr, "Open"},
        {5, nullptr, "Select"},
        {6, nullptr, "Poll"},
        {7, nullptr, "Sysctl"},
        {8, nullptr, "Recv"},
        {9, nullptr, "RecvFrom"},
        {10, nullptr, "Send"},
        {11, &BSD::SendTo, "SendTo"},
        {12, nullptr, "Accept"},
        {13, nullptr, "Bind"},
        {14, &BSD::Connect, "Connect"},
        {15, nullptr, "GetPeerName"},
        {16, nullptr, "GetSockName"},
        {17, nullptr, "GetSockOpt"},
        {18, nullptr, "Listen"},
        {19, nullptr, "Ioctl"},
        {20, nullptr, "Fcntl"},
        {21, nullptr, "SetSockOpt"},
        {22, nullptr, "Shutdown"},
        {23, nullptr, "ShutdownAllSockets"},
        {24, nullptr, "Write"},
        {25, nullptr, "Read"},
        {26, &BSD::Close, "Close"},
        {27, nullptr, "DuplicateSocket"},
        {28, nullptr, "GetResourceStatistics"},
        {29, nullptr, "RecvMMsg"},
        {30, nullptr, "SendMMsg"},
    };
    RegisterHandlers(functions);
}

BSDCFG::BSDCFG() : ServiceFramework{"bsdcfg"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "SetIfUp"},
        {1, nullptr, "SetIfUpWithEvent"},
        {2, nullptr, "CancelIf"},
        {3, nullptr, "SetIfDown"},
        {4, nullptr, "GetIfState"},
        {5, nullptr, "DhcpRenew"},
        {6, nullptr, "AddStaticArpEntry"},
        {7, nullptr, "RemoveArpEntry"},
        {8, nullptr, "LookupArpEntry"},
        {9, nullptr, "LookupArpEntry2"},
        {10, nullptr, "ClearArpEntries"},
        {11, nullptr, "ClearArpEntries2"},
        {12, nullptr, "PrintArpEntries"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

} // namespace Service::Sockets
