// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/service/btm/btm.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::BTM {

class BTM final : public ServiceFramework<BTM> {
public:
    explicit BTM() : ServiceFramework{"btm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "RegisterSystemEventForConnectedDeviceConditionImpl"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "RegisterSystemEventForRegisteredDeviceInfoImpl"},
            {9, nullptr, "Unknown8"},
            {10, nullptr, "Unknown9"},
            {11, nullptr, "Unknown10"},
            {12, nullptr, "Unknown11"},
            {13, nullptr, "Unknown12"},
            {14, nullptr, "EnableRadioImpl"},
            {15, nullptr, "DisableRadioImpl"},
            {16, nullptr, "Unknown13"},
            {17, nullptr, "Unknown14"},
            {18, nullptr, "Unknown15"},
            {19, nullptr, "Unknown16"},
            {20, nullptr, "Unknown17"},
            {21, nullptr, "Unknown18"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class BTM_DBG final : public ServiceFramework<BTM_DBG> {
public:
    explicit BTM_DBG() : ServiceFramework{"btm:dbg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "RegisterSystemEventForDiscoveryImpl"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "Unknown8"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class IBtmSystemCore final : public ServiceFramework<IBtmSystemCore> {
public:
    explicit IBtmSystemCore() : ServiceFramework{"IBtmSystemCore"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "StartGamepadPairingImpl"},
            {1, nullptr, "CancelGamepadPairingImpl"},
            {2, nullptr, "ClearGamepadPairingDatabaseImpl"},
            {3, nullptr, "GetPairedGamepadCountImpl"},
            {4, nullptr, "EnableRadioImpl"},
            {5, nullptr, "DisableRadioImpl"},
            {6, nullptr, "GetRadioOnOffImpl"},
            {7, nullptr, "AcquireRadioEventImpl"},
            {8, nullptr, "AcquireGamepadPairingEventImpl"},
            {9, nullptr, "IsGamepadPairingStartedImpl"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class BTM_SYS final : public ServiceFramework<BTM_SYS> {
public:
    explicit BTM_SYS() : ServiceFramework{"btm:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &BTM_SYS::GetCoreImpl, "GetCoreImpl"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCoreImpl(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IBtmSystemCore>();

        LOG_DEBUG(Service_BTM, "called");
    }
};

void InstallInterfaces(SM::ServiceManager& sm) {
    std::make_shared<BTM>()->InstallAsService(sm);
    std::make_shared<BTM_DBG>()->InstallAsService(sm);
    std::make_shared<BTM_SYS>()->InstallAsService(sm);
}

} // namespace Service::BTM
