// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/mig/mig.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::Migration {

class MIG_USR final : public ServiceFramework<MIG_USR> {
public:
    explicit MIG_USR() : ServiceFramework{"mig:usr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {10, nullptr, "TryGetLastMigrationInfo"},
            {100, nullptr, "CreateServer"},
            {101, nullptr, "ResumeServer"},
            {200, nullptr, "CreateClient"},
            {201, nullptr, "ResumeClient"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<MIG_USR>()->InstallAsService(sm);
}

} // namespace Service::Migration
