// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/btm/btm.h"
#include "core/hle/service/service.h"

namespace Service::BTM {

class IBtmUserCore final : public ServiceFramework<IBtmUserCore> {
public:
    explicit IBtmUserCore(Core::System& system) : ServiceFramework{"IBtmUserCore"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IBtmUserCore::AcquireBleScanEvent, "AcquireBleScanEvent"},
            {1, nullptr, "GetBleScanFilterParameter"},
            {2, nullptr, "GetBleScanFilterParameter2"},
            {3, nullptr, "StartBleScanForGeneral"},
            {4, nullptr, "StopBleScanForGeneral"},
            {5, nullptr, "GetBleScanResultsForGeneral"},
            {6, nullptr, "StartBleScanForPaired"},
            {7, nullptr, "StopBleScanForPaired"},
            {8, nullptr, "StartBleScanForSmartDevice"},
            {9, nullptr, "StopBleScanForSmartDevice"},
            {10, nullptr, "GetBleScanResultsForSmartDevice"},
            {17, &IBtmUserCore::AcquireBleConnectionEvent, "AcquireBleConnectionEvent"},
            {18, nullptr, "BleConnect"},
            {19, nullptr, "BleDisconnect"},
            {20, nullptr, "BleGetConnectionState"},
            {21, nullptr, "AcquireBlePairingEvent"},
            {22, nullptr, "BlePairDevice"},
            {23, nullptr, "BleUnPairDevice"},
            {24, nullptr, "BleUnPairDevice2"},
            {25, nullptr, "BleGetPairedDevices"},
            {26, &IBtmUserCore::AcquireBleServiceDiscoveryEvent, "AcquireBleServiceDiscoveryEvent"},
            {27, nullptr, "GetGattServices"},
            {28, nullptr, "GetGattService"},
            {29, nullptr, "GetGattIncludedServices"},
            {30, nullptr, "GetBelongingGattService"},
            {31, nullptr, "GetGattCharacteristics"},
            {32, nullptr, "GetGattDescriptors"},
            {33, &IBtmUserCore::AcquireBleMtuConfigEvent, "AcquireBleMtuConfigEvent"},
            {34, nullptr, "ConfigureBleMtu"},
            {35, nullptr, "GetBleMtu"},
            {36, nullptr, "RegisterBleGattDataPath"},
            {37, nullptr, "UnregisterBleGattDataPath"},
        };
        // clang-format on
        RegisterHandlers(functions);

        auto& kernel = system.Kernel();
        scan_event = Kernel::WritableEvent::CreateEventPair(kernel, "IBtmUserCore:ScanEvent");
        connection_event =
            Kernel::WritableEvent::CreateEventPair(kernel, "IBtmUserCore:ConnectionEvent");
        service_discovery =
            Kernel::WritableEvent::CreateEventPair(kernel, "IBtmUserCore:Discovery");
        config_event = Kernel::WritableEvent::CreateEventPair(kernel, "IBtmUserCore:ConfigEvent");
    }

private:
    void AcquireBleScanEvent(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_BTM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(scan_event.readable);
    }

    void AcquireBleConnectionEvent(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_BTM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(connection_event.readable);
    }

    void AcquireBleServiceDiscoveryEvent(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_BTM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(service_discovery.readable);
    }

    void AcquireBleMtuConfigEvent(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_BTM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(config_event.readable);
    }

    Kernel::EventPair scan_event;
    Kernel::EventPair connection_event;
    Kernel::EventPair service_discovery;
    Kernel::EventPair config_event;
};

class BTM_USR final : public ServiceFramework<BTM_USR> {
public:
    explicit BTM_USR(Core::System& system) : ServiceFramework{"btm:u"}, system(system) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &BTM_USR::GetCore, "GetCore"},
        };
        // clang-format on
        RegisterHandlers(functions);
    }

private:
    void GetCore(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BTM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IBtmUserCore>(system);
    }

    Core::System& system;
};

class BTM final : public ServiceFramework<BTM> {
public:
    explicit BTM() : ServiceFramework{"btm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "Unknown1"},
            {1, nullptr, "Unknown2"},
            {2, nullptr, "RegisterSystemEventForConnectedDeviceCondition"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "RegisterSystemEventForRegisteredDeviceInfo"},
            {9, nullptr, "Unknown8"},
            {10, nullptr, "Unknown9"},
            {11, nullptr, "Unknown10"},
            {12, nullptr, "Unknown11"},
            {13, nullptr, "Unknown12"},
            {14, nullptr, "EnableRadio"},
            {15, nullptr, "DisableRadio"},
            {16, nullptr, "Unknown13"},
            {17, nullptr, "Unknown14"},
            {18, nullptr, "Unknown15"},
            {19, nullptr, "Unknown16"},
            {20, nullptr, "Unknown17"},
            {21, nullptr, "Unknown18"},
            {22, nullptr, "Unknown19"},
            {23, nullptr, "Unknown20"},
            {24, nullptr, "Unknown21"},
            {25, nullptr, "Unknown22"},
            {26, nullptr, "Unknown23"},
            {27, nullptr, "Unknown24"},
            {28, nullptr, "Unknown25"},
            {29, nullptr, "Unknown26"},
            {30, nullptr, "Unknown27"},
            {31, nullptr, "Unknown28"},
            {32, nullptr, "Unknown29"},
            {33, nullptr, "Unknown30"},
            {34, nullptr, "Unknown31"},
            {35, nullptr, "Unknown32"},
            {36, nullptr, "Unknown33"},
            {37, nullptr, "Unknown34"},
            {38, nullptr, "Unknown35"},
            {39, nullptr, "Unknown36"},
            {40, nullptr, "Unknown37"},
            {41, nullptr, "Unknown38"},
            {42, nullptr, "Unknown39"},
            {43, nullptr, "Unknown40"},
            {44, nullptr, "Unknown41"},
            {45, nullptr, "Unknown42"},
            {46, nullptr, "Unknown43"},
            {47, nullptr, "Unknown44"},
            {48, nullptr, "Unknown45"},
            {49, nullptr, "Unknown46"},
            {50, nullptr, "Unknown47"},
            {51, nullptr, "Unknown48"},
            {52, nullptr, "Unknown49"},
            {53, nullptr, "Unknown50"},
            {54, nullptr, "Unknown51"},
            {55, nullptr, "Unknown52"},
            {56, nullptr, "Unknown53"},
            {57, nullptr, "Unknown54"},
            {58, nullptr, "Unknown55"},
            {59, nullptr, "Unknown56"},
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
            {0, nullptr, "RegisterSystemEventForDiscovery"},
            {1, nullptr, "Unknown1"},
            {2, nullptr, "Unknown2"},
            {3, nullptr, "Unknown3"},
            {4, nullptr, "Unknown4"},
            {5, nullptr, "Unknown5"},
            {6, nullptr, "Unknown6"},
            {7, nullptr, "Unknown7"},
            {8, nullptr, "Unknown8"},
            {9, nullptr, "Unknown9"},
            {10, nullptr, "Unknown10"},
            {11, nullptr, "Unknown11"},
            {12, nullptr, "Unknown11"},
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
            {0, nullptr, "StartGamepadPairing"},
            {1, nullptr, "CancelGamepadPairing"},
            {2, nullptr, "ClearGamepadPairingDatabase"},
            {3, nullptr, "GetPairedGamepadCount"},
            {4, nullptr, "EnableRadio"},
            {5, nullptr, "DisableRadio"},
            {6, nullptr, "GetRadioOnOff"},
            {7, nullptr, "AcquireRadioEvent"},
            {8, nullptr, "AcquireGamepadPairingEvent"},
            {9, nullptr, "IsGamepadPairingStarted"},
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
            {0, &BTM_SYS::GetCore, "GetCore"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void GetCore(Kernel::HLERequestContext& ctx) {
        LOG_DEBUG(Service_BTM, "called");

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IBtmSystemCore>();
    }
};

void InstallInterfaces(SM::ServiceManager& sm, Core::System& system) {
    std::make_shared<BTM>()->InstallAsService(sm);
    std::make_shared<BTM_DBG>()->InstallAsService(sm);
    std::make_shared<BTM_SYS>()->InstallAsService(sm);
    std::make_shared<BTM_USR>(system)->InstallAsService(sm);
}

} // namespace Service::BTM
