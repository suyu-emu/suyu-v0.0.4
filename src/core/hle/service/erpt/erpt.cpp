// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/erpt/erpt.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::ERPT {

class ErrorReportContext final : public ServiceFramework<ErrorReportContext> {
public:
    explicit ErrorReportContext(Core::System& system_) : ServiceFramework{system_, "erpt:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "SubmitContext"},
            {1, nullptr, "CreateReport"},
            {2, nullptr, "SetInitialLaunchSettingsCompletionTime"},
            {3, nullptr, "ClearInitialLaunchSettingsCompletionTime"},
            {4, nullptr, "UpdatePowerOnTime"},
            {5, nullptr, "UpdateAwakeTime"},
            {6, nullptr, "SubmitMultipleCategoryContext"},
            {7, nullptr, "UpdateApplicationLaunchTime"},
            {8, nullptr, "ClearApplicationLaunchTime"},
            {9, nullptr, "SubmitAttachment"},
            {10, nullptr, "CreateReportWithAttachments"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class ErrorReportSession final : public ServiceFramework<ErrorReportSession> {
public:
    explicit ErrorReportSession(Core::System& system_) : ServiceFramework{system_, "erpt:r"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "OpenReport"},
            {1, nullptr, "OpenManager"},
            {2, nullptr, "OpenAttachment"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm, Core::System& system) {
    std::make_shared<ErrorReportContext>(system)->InstallAsService(sm);
    std::make_shared<ErrorReportSession>(system)->InstallAsService(sm);
}

} // namespace Service::ERPT
