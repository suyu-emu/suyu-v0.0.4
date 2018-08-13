// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/service/mm/mm_u.h"

namespace Service::MM {

class MM_U final : public ServiceFramework<MM_U> {
public:
    explicit MM_U() : ServiceFramework{"mm:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "InitializeOld"},
            {1, nullptr, "FinalizeOld"},
            {2, nullptr, "SetAndWaitOld"},
            {3, nullptr, "GetOld"},
            {4, &MM_U::Initialize, "Initialize"},
            {5, nullptr, "Finalize"},
            {6, &MM_U::SetAndWait, "SetAndWait"},
            {7, &MM_U::Get, "Get"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void Initialize(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_MM, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void SetAndWait(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        min = rp.Pop<u32>();
        max = rp.Pop<u32>();
        current = min;

        LOG_WARNING(Service_MM, "(STUBBED) called, min=0x{:X}, max=0x{:X}", min, max);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    void Get(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_MM, "(STUBBED) called");
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(current);
    }

    u32 min{0};
    u32 max{0};
    u32 current{0};
};

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<MM_U>()->InstallAsService(service_manager);
}

} // namespace Service::MM
