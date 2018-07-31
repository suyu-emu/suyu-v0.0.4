// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/fgm/fgm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::FGM {

class IRequest final : public ServiceFramework<IRequest> {
public:
    explicit IRequest() : ServiceFramework{"IRequest"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Set"},
            {2, nullptr, "Get"},
            {3, nullptr, "Cancel"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class FGM final : public ServiceFramework<FGM> {
public:
    explicit FGM(const char* name) : ServiceFramework{name} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &FGM::Initialize, "Initialize"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Initialize(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IRequest>();

        LOG_DEBUG(Service_FGM, "called");
    }
};

class FGM_DBG final : public ServiceFramework<FGM_DBG> {
public:
    explicit FGM_DBG() : ServiceFramework{"fgm:dbg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Initialize"},
            {1, nullptr, "Read"},
            {2, nullptr, "Cancel"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<FGM>("fgm")->InstallAsService(sm);
    std::make_shared<FGM>("fgm:0")->InstallAsService(sm);
    std::make_shared<FGM>("fgm:9")->InstallAsService(sm);
    std::make_shared<FGM_DBG>()->InstallAsService(sm);
}

} // namespace Service::FGM
