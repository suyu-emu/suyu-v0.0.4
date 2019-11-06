// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/readable_event.h"
#include "core/hle/kernel/writable_event.h"
#include "core/hle/service/btdrv/btdrv.h"
#include "core/hle/service/service.h"
#include "core/hle/service/sm/sm.h"

namespace Service::BtDrv {

class Bt final : public ServiceFramework<Bt> {
public:
    explicit Bt(Core::System& system) : ServiceFramework{"bt"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "LeClientReadCharacteristic"},
            {1, nullptr, "LeClientReadDescriptor"},
            {2, nullptr, "LeClientWriteCharacteristic"},
            {3, nullptr, "LeClientWriteDescriptor"},
            {4, nullptr, "LeClientRegisterNotification"},
            {5, nullptr, "LeClientDeregisterNotification"},
            {6, nullptr, "SetLeResponse"},
            {7, nullptr, "LeSendIndication"},
            {8, nullptr, "GetLeEventInfo"},
            {9, &Bt::RegisterBleEvent, "RegisterBleEvent"},
        };
        // clang-format on
        RegisterHandlers(functions);

        auto& kernel = system.Kernel();
        register_event = Kernel::WritableEvent::CreateEventPair(kernel, "BT:RegisterEvent");
    }

private:
    void RegisterBleEvent(Kernel::HLERequestContext& ctx) {
        LOG_WARNING(Service_BTM, "(STUBBED) called");

        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(register_event.readable);
    }

    Kernel::EventPair register_event;
};

class BtDrv final : public ServiceFramework<BtDrv> {
public:
    explicit BtDrv() : ServiceFramework{"btdrv"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "InitializeBluetoothDriver"},
            {1, nullptr, "InitializeBluetooth"},
            {2, nullptr, "EnableBluetooth"},
            {3, nullptr, "DisableBluetooth"},
            {4, nullptr, "CleanupBluetooth"},
            {5, nullptr, "GetAdapterProperties"},
            {6, nullptr, "GetAdapterProperty"},
            {7, nullptr, "SetAdapterProperty"},
            {8, nullptr, "StartDiscovery"},
            {9, nullptr, "CancelDiscovery"},
            {10, nullptr, "CreateBond"},
            {11, nullptr, "RemoveBond"},
            {12, nullptr, "CancelBond"},
            {13, nullptr, "PinReply"},
            {14, nullptr, "SspReply"},
            {15, nullptr, "GetEventInfo"},
            {16, nullptr, "InitializeHid"},
            {17, nullptr, "HidConnect"},
            {18, nullptr, "HidDisconnect"},
            {19, nullptr, "HidSendData"},
            {20, nullptr, "HidSendData2"},
            {21, nullptr, "HidSetReport"},
            {22, nullptr, "HidGetReport"},
            {23, nullptr, "HidWakeController"},
            {24, nullptr, "HidAddPairedDevice"},
            {25, nullptr, "HidGetPairedDevice"},
            {26, nullptr, "CleanupHid"},
            {27, nullptr, "HidGetEventInfo"},
            {28, nullptr, "ExtSetTsi"},
            {29, nullptr, "ExtSetBurstMode"},
            {30, nullptr, "ExtSetZeroRetran"},
            {31, nullptr, "ExtSetMcMode"},
            {32, nullptr, "ExtStartLlrMode"},
            {33, nullptr, "ExtExitLlrMode"},
            {34, nullptr, "ExtSetRadio"},
            {35, nullptr, "ExtSetVisibility"},
            {36, nullptr, "ExtSetTbfcScan"},
            {37, nullptr, "RegisterHidReportEvent"},
            {38, nullptr, "HidGetReportEventInfo"},
            {39, nullptr, "GetLatestPlr"},
            {40, nullptr, "ExtGetPendingConnections"},
            {41, nullptr, "GetChannelMap"},
            {42, nullptr, "EnableBluetoothBoostSetting"},
            {43, nullptr, "IsBluetoothBoostSettingEnabled"},
            {44, nullptr, "EnableBluetoothAfhSetting"},
            {45, nullptr, "IsBluetoothAfhSettingEnabled"},
            {46, nullptr, "InitializeBluetoothLe"},
            {47, nullptr, "EnableBluetoothLe"},
            {48, nullptr, "DisableBluetoothLe"},
            {49, nullptr, "CleanupBluetoothLe"},
            {50, nullptr, "SetLeVisibility"},
            {51, nullptr, "SetLeConnectionParameter"},
            {52, nullptr, "SetLeDefaultConnectionParameter"},
            {53, nullptr, "SetLeAdvertiseData"},
            {54, nullptr, "SetLeAdvertiseParameter"},
            {55, nullptr, "StartLeScan"},
            {56, nullptr, "StopLeScan"},
            {57, nullptr, "AddLeScanFilterCondition"},
            {58, nullptr, "DeleteLeScanFilterCondition"},
            {59, nullptr, "DeleteLeScanFilter"},
            {60, nullptr, "ClearLeScanFilters"},
            {61, nullptr, "EnableLeScanFilter"},
            {62, nullptr, "RegisterLeClient"},
            {63, nullptr, "UnregisterLeClient"},
            {64, nullptr, "UnregisterLeClientAll"},
            {65, nullptr, "LeClientConnect"},
            {66, nullptr, "LeClientCancelConnection"},
            {67, nullptr, "LeClientDisconnect"},
            {68, nullptr, "LeClientGetAttributes"},
            {69, nullptr, "LeClientDiscoverService"},
            {70, nullptr, "LeClientConfigureMtu"},
            {71, nullptr, "RegisterLeServer"},
            {72, nullptr, "UnregisterLeServer"},
            {73, nullptr, "LeServerConnect"},
            {74, nullptr, "LeServerDisconnect"},
            {75, nullptr, "CreateLeService"},
            {76, nullptr, "StartLeService"},
            {77, nullptr, "AddLeCharacteristic"},
            {78, nullptr, "AddLeDescriptor"},
            {79, nullptr, "GetLeCoreEventInfo"},
            {80, nullptr, "LeGetFirstCharacteristic"},
            {81, nullptr, "LeGetNextCharacteristic"},
            {82, nullptr, "LeGetFirstDescriptor"},
            {83, nullptr, "LeGetNextDescriptor"},
            {84, nullptr, "RegisterLeCoreDataPath"},
            {85, nullptr, "UnregisterLeCoreDataPath"},
            {86, nullptr, "RegisterLeHidDataPath"},
            {87, nullptr, "UnregisterLeHidDataPath"},
            {88, nullptr, "RegisterLeDataPath"},
            {89, nullptr, "UnregisterLeDataPath"},
            {90, nullptr, "LeClientReadCharacteristic"},
            {91, nullptr, "LeClientReadDescriptor"},
            {92, nullptr, "LeClientWriteCharacteristic"},
            {93, nullptr, "LeClientWriteDescriptor"},
            {94, nullptr, "LeClientRegisterNotification"},
            {95, nullptr, "LeClientDeregisterNotification"},
            {96, nullptr, "GetLeHidEventInfo"},
            {97, nullptr, "RegisterBleHidEvent"},
            {98, nullptr, "SetLeScanParameter"},
            {256, nullptr, "GetIsManufacturingMode"},
            {257, nullptr, "EmulateBluetoothCrash"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void InstallInterfaces(SM::ServiceManager& sm, Core::System& system) {
    std::make_shared<BtDrv>()->InstallAsService(sm);
    std::make_shared<Bt>(system)->InstallAsService(sm);
}

} // namespace Service::BtDrv
