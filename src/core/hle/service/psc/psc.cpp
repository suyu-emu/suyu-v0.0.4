// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/psc/psc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::PSC {

class PSC_C final : public ServiceFramework<PSC_C> {
public:
    explicit PSC_C() : ServiceFramework{"psc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
            {6, nullptr, "Unknown7"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IPmModule final : public ServiceFramework<IPmModule> {
public:
    explicit IPmModule() : ServiceFramework{"IPmModule"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "GetRequest"},
            {2, nullptr, "Acknowledge"},
            {3, nullptr, "Unknown1"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class PSC_M final : public ServiceFramework<PSC_M> {
public:
    explicit PSC_M() : ServiceFramework{"psc:m"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &PSC_M::GetPmModule, "GetPmModule"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetPmModule(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_PSC, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IPmModule>();
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<PSC_C>()->InstallAsService(sm);
    std::make_shared<PSC_M>()->InstallAsService(sm);
}

} // namespace Service::PSC
