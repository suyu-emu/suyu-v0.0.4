// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/ncm/ncm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::NCM {

class LocationResolver final : public ServiceFramework<LocationResolver> {
public:
    explicit LocationResolver() : ServiceFramework{"lr"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "OpenLocationResolver"},
            {1, nullptr, "OpenRegisteredLocationResolver"},
            {2, nullptr, "RefreshLocationResolver"},
            {3, nullptr, "OpenAddOnContentLocationResolver"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NCM final : public ServiceFramework<NCM> {
public:
    explicit NCM() : ServiceFramework{"ncm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "CreateContentStorage"},
            {1, nullptr, "CreateContentMetaDatabase"},
            {2, nullptr, "VerifyContentStorage"},
            {3, nullptr, "VerifyContentMetaDatabase"},
            {4, nullptr, "OpenContentStorage"},
            {5, nullptr, "OpenContentMetaDatabase"},
            {6, nullptr, "CloseContentStorageForcibly"},
            {7, nullptr, "CloseContentMetaDatabaseForcibly"},
            {8, nullptr, "CleanupContentMetaDatabase"},
            {9, nullptr, "OpenContentStorage2"},
            {10, nullptr, "CloseContentStorage"},
            {11, nullptr, "OpenContentMetaDatabase2"},
            {12, nullptr, "CloseContentMetaDatabase"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<LocationResolver>()->InstallAsService(sm);
    std::make_shared<NCM>()->InstallAsService(sm);
}

} // namespace Service::NCM
