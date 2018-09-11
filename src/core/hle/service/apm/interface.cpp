// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/apm/apm.h"
#include "core/hle/service/apm/interface.h"

namespace Service::APM {

class ISession final : public ServiceFramework<ISession> {
public:
    ISession() : ServiceFramework("ISession") {
        static const FunctionInfo functions[] = {
            {0, &ISession::SetPerformanceConfiguration, "SetPerformanceConfiguration"},
            {1, &ISession::GetPerformanceConfiguration, "GetPerformanceConfiguration"},
        };
        RegisterHandlers(functions);
    }

private:
    enum class PerformanceConfiguration : u32 {
        Config1 = 0x00010000,
        Config2 = 0x00010001,
        Config3 = 0x00010002,
        Config4 = 0x00020000,
        Config5 = 0x00020001,
        Config6 = 0x00020002,
        Config7 = 0x00020003,
        Config8 = 0x00020004,
        Config9 = 0x00020005,
        Config10 = 0x00020006,
        Config11 = 0x92220007,
        Config12 = 0x92220008,
    };

    void SetPerformanceConfiguration(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto mode = static_cast<PerformanceMode>(rp.Pop<u32>());
        u32 config = rp.Pop<u32>();

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);

        LOG_WARNING(Service_APM, "(STUBBED) called mode={} config={}", static_cast<u32>(mode),
                    config);
    }

    void GetPerformanceConfiguration(Kernel::HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};

        auto mode = static_cast<PerformanceMode>(rp.Pop<u32>());

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(static_cast<u32>(PerformanceConfiguration::Config1));

        LOG_WARNING(Service_APM, "(STUBBED) called mode={}", static_cast<u32>(mode));
    }
};

APM::APM(std::shared_ptr<Module> apm, const char* name)
    : ServiceFramework(name), apm(std::move(apm)) {
    static const FunctionInfo functions[] = {
        {0, &APM::OpenSession, "OpenSession"},
        {1, nullptr, "GetPerformanceMode"},
    };
    RegisterHandlers(functions);
}

APM::~APM() = default;

void APM::OpenSession(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISession>();

    LOG_DEBUG(Service_APM, "called");
}

APM_Sys::APM_Sys() : ServiceFramework{"apm:sys"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "RequestPerformanceMode"},
        {1, &APM_Sys::GetPerformanceEvent, "GetPerformanceEvent"},
        {2, nullptr, "GetThrottlingState"},
        {3, nullptr, "GetLastThrottlingState"},
        {4, nullptr, "ClearLastThrottlingState"},
        {5, nullptr, "LoadAndApplySettings"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

APM_Sys::~APM_Sys() = default;

void APM_Sys::GetPerformanceEvent(Kernel::HLERequestContext& ctx) {
    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(RESULT_SUCCESS);
    rb.PushIpcInterface<ISession>();

    LOG_DEBUG(Service_APM, "called");
}

} // namespace Service::APM
