// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/grc/grc.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::GRC {

class GRC final : public ServiceFramework<GRC> {
public:
    explicit GRC() : ServiceFramework{"grc:c"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "OpenContinuousRecorder"},
            {2, nullptr, "OpenGameMovieTrimmer"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<GRC>()->InstallAsService(sm);
}

} // namespace Service::GRC
