// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/pm/pm.h"
#include "core/hle/service/service.h"

namespace Service::PM {

class BootMode final : public ServiceFramework<BootMode> {
public:
    explicit BootMode() : ServiceFramework{"pm:bm"} {
        static const FunctionInfo functions[] = {
            {0, &BootMode::GetBootMode, "GetBootMode"},
            {1, &BootMode::SetMaintenanceBoot, "SetMaintenanceBoot"},
        };
        RegisterHandlers(functions);
    }

private:
    void GetBootMode(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_PM, "called");

        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.PushEnum(boot_mode);
    }

    void SetMaintenanceBoot(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_PM, "called");

        boot_mode = SystemBootMode::Maintenance;

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
    }

    SystemBootMode boot_mode = SystemBootMode::Normal;
};

class DebugMonitor final : public ServiceFramework<DebugMonitor> {
public:
    explicit DebugMonitor() : ServiceFramework{"pm:dmnt"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetDebugProcesses"},
            {1, nullptr, "StartDebugProcess"},
            {2, nullptr, "GetTitlePid"},
            {3, nullptr, "EnableDebugForTitleId"},
            {4, nullptr, "GetApplicationPid"},
            {5, nullptr, "EnableDebugForApplication"},
            {6, nullptr, "DisableDebug"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class Info final : public ServiceFramework<Info> {
public:
    explicit Info() : ServiceFramework{"pm:info"} {
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetTitleId"},
        };
        RegisterHandlers(functions);
    }
};

class Shell final : public ServiceFramework<Shell> {
public:
    explicit Shell() : ServiceFramework{"pm:shell"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "LaunchProcess"},
            {1, nullptr, "TerminateProcessByPid"},
            {2, nullptr, "TerminateProcessByTitleId"},
            {3, nullptr, "GetProcessEventWaiter"},
            {4, nullptr, "GetProcessEventType"},
            {5, nullptr, "NotifyBootFinished"},
            {6, nullptr, "GetApplicationPid"},
            {7, nullptr, "BoostSystemMemoryResourceLimit"},
            {8, nullptr, "EnableAdditionalSystemThreads"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<BootMode>()->InstallAsService(sm);
    std::make_shared<DebugMonitor>()->InstallAsService(sm);
    std::make_shared<Info>()->InstallAsService(sm);
    std::make_shared<Shell>()->InstallAsService(sm);
}

} // namespace Service::PM
