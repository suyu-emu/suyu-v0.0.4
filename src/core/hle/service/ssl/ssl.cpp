// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/ssl/ssl.h"

namespace Service::SSL {

class ISslConnection final : public ServiceFramework<ISslConnection> {
public:
    ISslConnection() : ServiceFramework("ISslConnection") {
        static const FunctionInfo functions[] = {
            {0, nullptr, "SetSocketDescriptor"},
            {1, nullptr, "SetHostName"},
            {2, nullptr, "SetVerifyOption"},
            {3, nullptr, "SetIoMode"},
            {4, nullptr, "GetSocketDescriptor"},
            {5, nullptr, "GetHostName"},
            {6, nullptr, "GetVerifyOption"},
            {7, nullptr, "GetIoMode"},
            {8, nullptr, "DoHandshake"},
            {9, nullptr, "DoHandshakeGetServerCert"},
            {10, nullptr, "Read"},
            {11, nullptr, "Write"},
            {12, nullptr, "Pending"},
            {13, nullptr, "Peek"},
            {14, nullptr, "Poll"},
            {15, nullptr, "GetVerifyCertError"},
            {16, nullptr, "GetNeededServerCertBufferSize"},
            {17, nullptr, "SetSessionCacheMode"},
            {18, nullptr, "GetSessionCacheMode"},
            {19, nullptr, "FlushSessionCache"},
            {20, nullptr, "SetRenegotiationMode"},
            {21, nullptr, "GetRenegotiationMode"},
            {22, nullptr, "SetOption"},
            {23, nullptr, "GetOption"},
            {24, nullptr, "GetVerifyCertErrors"},
            {25, nullptr, "GetCipherInfo"},
        };
        RegisterHandlers(functions);
    }
};

class ISslContext final : public ServiceFramework<ISslContext> {
public:
    ISslContext() : ServiceFramework("ISslContext") {
        static const FunctionInfo functions[] = {
            {0, &ISslContext::SetOption, "SetOption"},
            {1, nullptr, "GetOption"},
            {2, &ISslContext::CreateConnection, "CreateConnection"},
            {3, nullptr, "GetConnectionCount"},
            {4, nullptr, "ImportServerPki"},
            {5, nullptr, "ImportClientPki"},
            {6, nullptr, "RemoveServerPki"},
            {7, nullptr, "RemoveClientPki"},
            {8, nullptr, "RegisterInternalPki"},
            {9, nullptr, "AddPolicyOid"},
            {10, nullptr, "ImportCrl"},
            {11, nullptr, "RemoveCrl"},
        };
        RegisterHandlers(functions);
    }
    ~ISslContext() = default;

private:
    void SetOption(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_SSL, "(STUBBED) called");
        IPC::RequestParser rp{ctx};

        IPC::ResponseBuilder rb = rp.MakeBuilder(2, 0, 0);
        rb.Push(RESULT_SUCCESS);
    }

    void CreateConnection(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_SSL, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<ISslConnection>();
    }
};

class SSL final : public ServiceFramework<SSL> {
public:
    explicit SSL() : ServiceFramework{"ssl"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &SSL::CreateContext, "CreateContext"},
            {1, nullptr, "GetContextCount"},
            {2, nullptr, "GetCertificates"},
            {3, nullptr, "GetCertificateBufSize"},
            {4, nullptr, "DebugIoctl"},
            {5, &SSL::SetInterfaceVersion, "SetInterfaceVersion"},
            {6, nullptr, "FlushSessionCache"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void CreateContext(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_SSL, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<ISslContext>();
    }

    void SetInterfaceVersion(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_SSL, "(STUBBED) called");
        IPC::RequestParser rp{ctx};
        u32 unk1 = rp.Pop<u32>(); // Probably minor/major?
        u32 unk2 = rp.Pop<u32>(); // TODO(ogniK): Figure out what this does

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }
};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<SSL>()->InstallAsService(service_manager);
}

} // namespace Service::SSL
