// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/npns/npns.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::NPNS {

class NPNS_S final : public ServiceFramework<NPNS_S> {
public:
    explicit NPNS_S() : ServiceFramework{"npns:s"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "ListenAll"},
            {2, nullptr, "ListenTo"},
            {3, nullptr, "Receive"},
            {4, nullptr, "ReceiveRaw"},
            {5, nullptr, "GetReceiveEvent"},
            {6, nullptr, "ListenUndelivered"},
            {7, nullptr, "GetStateChangeEVent"},
            {11, nullptr, "SubscribeTopic"},
            {12, nullptr, "UnsubscribeTopic"},
            {13, nullptr, "QueryIsTopicExist"},
            {21, nullptr, "CreateToken"},
            {22, nullptr, "CreateTokenWithApplicationId"},
            {23, nullptr, "DestroyToken"},
            {24, nullptr, "DestroyTokenWithApplicationId"},
            {25, nullptr, "QueryIsTokenValid"},
            {31, nullptr, "UploadTokenToBaaS"},
            {32, nullptr, "DestroyTokenForBaaS"},
            {33, nullptr, "CreateTokenForBaaS"},
            {34, nullptr, "SetBaaSDeviceAccountIdList"},
            {101, nullptr, "Suspend"},
            {102, nullptr, "Resume"},
            {103, nullptr, "GetState"},
            {104, nullptr, "GetStatistics"},
            {105, nullptr, "GetPlayReportRequestEvent"},
            {111, nullptr, "GetJid"},
            {112, nullptr, "CreateJid"},
            {113, nullptr, "DestroyJid"},
            {114, nullptr, "AttachJid"},
            {115, nullptr, "DetachJid"},
            {201, nullptr, "RequestChangeStateForceTimed"},
            {102, nullptr, "RequestChangeStateForceAsync"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class NPNS_U final : public ServiceFramework<NPNS_U> {
public:
    explicit NPNS_U() : ServiceFramework{"npns:u"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "ListenAll"},
            {2, nullptr, "ListenTo"},
            {3, nullptr, "Receive"},
            {4, nullptr, "ReceiveRaw"},
            {5, nullptr, "GetReceiveEvent"},
            {7, nullptr, "GetStateChangeEVent"},
            {21, nullptr, "CreateToken"},
            {23, nullptr, "DestroyToken"},
            {25, nullptr, "QueryIsTokenValid"},
            {26, nullptr, "ListenToMyApplicationId"},
            {101, nullptr, "Suspend"},
            {102, nullptr, "Resume"},
            {103, nullptr, "GetState"},
            {104, nullptr, "GetStatistics"},
            {111, nullptr, "GetJid"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<NPNS_S>()->InstallAsService(sm);
    std::make_shared<NPNS_U>()->InstallAsService(sm);
}

} // namespace Service::NPNS
