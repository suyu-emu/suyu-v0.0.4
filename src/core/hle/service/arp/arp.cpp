// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/arp/arp.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::ARP {

class ARP_R final : public ServiceFramework<ARP_R> {
public:
    explicit ARP_R() : ServiceFramework{"arp:r"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetApplicationLaunchProperty"},
            {1, nullptr, "GetApplicationLaunchPropertyWithApplicationId"},
            {2, nullptr, "GetApplicationControlProperty"},
            {3, nullptr, "GetApplicationControlPropertyWithApplicationId"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IRegistrar final : public ServiceFramework<IRegistrar> {
public:
    explicit IRegistrar() : ServiceFramework{"IRegistrar"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Issue"},
            {1, nullptr, "SetApplicationLaunchProperty"},
            {2, nullptr, "SetApplicationControlProperty"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ARP_W final : public ServiceFramework<ARP_W> {
public:
    explicit ARP_W() : ServiceFramework{"arp:w"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &ARP_W::AcquireRegistrar, "AcquireRegistrar"},
            {1, nullptr, "DeleteProperties"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void AcquireRegistrar(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_ARP, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IRegistrar>();
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<ARP_R>()->InstallAsService(sm);
    std::make_shared<ARP_W>()->InstallAsService(sm);
}

} // namespace Service::ARP
