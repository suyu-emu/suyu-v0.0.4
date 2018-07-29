// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/wlan/wlan.h"

namespace Service::WLAN {

class WLANInfra final : public ServiceFramework<WLANInfra> {
public:
    explicit WLANInfra() : ServiceFramework{"wlan:inf"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "GetMacAddress"},
            {3, nullptr, "StartScan"},
            {4, nullptr, "StopScan"},
            {5, nullptr, "Connect"},
            {6, nullptr, "CancelConnect"},
            {7, nullptr, "Disconnect"},
            {8, nullptr, "Unknown3"},
            {9, nullptr, "Unknown4"},
            {10, nullptr, "GetState"},
            {11, nullptr, "GetScanResult"},
            {12, nullptr, "GetRssi"},
            {13, nullptr, "ChangeRxAntenna"},
            {14, nullptr, "Unknown5"},
            {15, nullptr, "Unknown6"},
            {16, nullptr, "RequestWakeUp"},
            {17, nullptr, "RequestIfUpDown"},
            {18, nullptr, "Unknown7"},
            {19, nullptr, "Unknown8"},
            {20, nullptr, "Unknown9"},
            {21, nullptr, "Unknown10"},
            {22, nullptr, "Unknown11"},
            {23, nullptr, "Unknown12"},
            {24, nullptr, "Unknown13"},
            {25, nullptr, "Unknown14"},
            {26, nullptr, "Unknown15"},
            {27, nullptr, "Unknown16"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class WLANLocal final : public ServiceFramework<WLANLocal> {
public:
    explicit WLANLocal() : ServiceFramework{"wlan:lcl"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
            {6, nullptr, "GetMacAddress"},
            {7, nullptr, "CreateBss"},
            {8, nullptr, "DestroyBss"},
            {9, nullptr, "StartScan"},
            {10, nullptr, "StopScan"},
            {11, nullptr, "Connect"},
            {12, nullptr, "CancelConnect"},
            {13, nullptr, "Join"},
            {14, nullptr, "CancelJoin"},
            {15, nullptr, "Disconnect"},
            {16, nullptr, "SetBeaconLostCount"},
            {17, nullptr, "Unknown7"},
            {18, nullptr, "Unknown8"},
            {19, nullptr, "Unknown9"},
            {20, nullptr, "GetBssIndicationEvent"},
            {21, nullptr, "GetBssIndicationInfo"},
            {22, nullptr, "GetState"},
            {23, nullptr, "GetAllowedChannels"},
            {24, nullptr, "AddIe"},
            {25, nullptr, "DeleteIe"},
            {26, nullptr, "Unknown10"},
            {27, nullptr, "Unknown11"},
            {28, nullptr, "CreateRxEntry"},
            {29, nullptr, "DeleteRxEntry"},
            {30, nullptr, "Unknown12"},
            {31, nullptr, "Unknown13"},
            {32, nullptr, "AddMatchingDataToRxEntry"},
            {33, nullptr, "RemoveMatchingDataFromRxEntry"},
            {34, nullptr, "GetScanResult"},
            {35, nullptr, "Unknown14"},
            {36, nullptr, "SetActionFrameWithBeacon"},
            {37, nullptr, "CancelActionFrameWithBeacon"},
            {38, nullptr, "CreateRxEntryForActionFrame"},
            {39, nullptr, "DeleteRxEntryForActionFrame"},
            {40, nullptr, "Unknown15"},
            {41, nullptr, "Unknown16"},
            {42, nullptr, "CancelGetActionFrame"},
            {43, nullptr, "GetRssi"},
            {44, nullptr, "Unknown17"},
            {45, nullptr, "Unknown18"},
            {46, nullptr, "Unknown19"},
            {47, nullptr, "Unknown20"},
            {48, nullptr, "Unknown21"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class WLANLocalGetFrame final : public ServiceFramework<WLANLocalGetFrame> {
public:
    explicit WLANLocalGetFrame() : ServiceFramework{"wlan:lg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class WLANSocketGetFrame final : public ServiceFramework<WLANSocketGetFrame> {
public:
    explicit WLANSocketGetFrame() : ServiceFramework{"wlan:sg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class WLANSocketManager final : public ServiceFramework<WLANSocketManager> {
public:
    explicit WLANSocketManager() : ServiceFramework{"wlan:soc"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "Unknown3"},
            {3, nullptr, "Unknown4"},
            {4, nullptr, "Unknown5"},
            {5, nullptr, "Unknown6"},
            {6, nullptr, "GetMacAddress"},
            {7, nullptr, "SwitchTsfTimerFunction"},
            {8, nullptr, "Unknown7"},
            {9, nullptr, "Unknown8"},
            {10, nullptr, "Unknown9"},
            {11, nullptr, "Unknown10"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<WLANInfra>()->InstallAsService(sm);
    std::make_shared<WLANLocal>()->InstallAsService(sm);
    std::make_shared<WLANLocalGetFrame>()->InstallAsService(sm);
    std::make_shared<WLANSocketGetFrame>()->InstallAsService(sm);
    std::make_shared<WLANSocketManager>()->InstallAsService(sm);
}

} // namespace Service::WLAN
