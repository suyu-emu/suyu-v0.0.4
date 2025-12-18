// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <memory>

#include "core/hle/service/btm/btm.h"
#include "core/hle/service/btm/btm_debug.h"
#include "core/hle/service/btm/btm_system.h"
#include "core/hle/service/btm/btm_user.h"
#include "core/hle/service/server_manager.h"
#include "core/hle/service/service.h"

namespace Service::BTM {

class IBtm final : public ServiceFramework<IBtm> {
public:
    explicit IBtm(Core::System& system_) : ServiceFramework{system_, "btm"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetState"},
            {1, nullptr, "GetHostDeviceProperty"},
            {2, nullptr, "AcquireDeviceConditionEvent"},
            {3, nullptr, "GetDeviceCondition"},
            {4, nullptr, "SetBurstMode"},
            {5, nullptr, "SetSlotMode"},
            {6, nullptr, "SetBluetoothMode"},
            {7, nullptr, "SetWlanMode"},
            {8, nullptr, "AcquireDeviceInfoEvent"},
            {9, nullptr, "GetDeviceInfo"},
            {10, nullptr, "AddDeviceInfo"},
            {11, nullptr, "RemoveDeviceInfo"},
            {12, nullptr, "IncreaseDeviceInfoOrder"},
            {13, nullptr, "LlrNotify"},
            {14, nullptr, "EnableRadio"},
            {15, nullptr, "DisableRadio"},
            {16, nullptr, "HidDisconnect"},
            {17, nullptr, "HidSetRetransmissionMode"},
            {18, nullptr, "AcquireAwakeReqEvent"},
            {19, nullptr, "AcquireLlrStateEvent"},
            {20, nullptr, "IsLlrStarted"},
            {21, nullptr, "EnableSlotSaving"},
            {22, nullptr, "ProtectDeviceInfo"},
            {23, nullptr, "AcquireBleScanEvent"},
            {24, nullptr, "GetBleScanParameterGeneral"},
            {25, nullptr, "GetBleScanParameterSmartDevice"},
            {26, nullptr, "StartBleScanForGeneral"},
            {27, nullptr, "StopBleScanForGeneral"},
            {28, nullptr, "GetBleScanResultsForGeneral"},
            {29, nullptr, "StartBleScanForPairedDevice"},
            {30, nullptr, "StopBleScanForPairedDevice"},
            {31, nullptr, "StartBleScanForSmartDevice"},
            {32, nullptr, "StopBleScanForSmartDevice"},
            {33, nullptr, "GetBleScanResultsForSmartDevice"},
            {34, nullptr, "AcquireBleConnectionEvent"},
            {35, nullptr, "BleConnect"},
            {36, nullptr, "BleOverrideConnection"},
            {37, nullptr, "BleDisconnect"},
            {38, nullptr, "BleGetConnectionState"},
            {39, nullptr, "BleGetGattClientConditionList"},
            {40, nullptr, "AcquireBlePairingEvent"},
            {41, nullptr, "BlePairDevice"},
            {42, nullptr, "BleUnpairDeviceOnBoth"},
            {43, nullptr, "BleUnpairDevice"},
            {44, nullptr, "BleGetPairedAddresses"},
            {45, nullptr, "AcquireBleServiceDiscoveryEvent"},
            {46, nullptr, "GetGattServices"},
            {47, nullptr, "GetGattService"},
            {48, nullptr, "GetGattIncludedServices"},
            {49, nullptr, "GetBelongingService"},
            {50, nullptr, "GetGattCharacteristics"},
            {51, nullptr, "GetGattDescriptors"},
            {52, nullptr, "AcquireBleMtuConfigEvent"},
            {53, nullptr, "ConfigureBleMtu"},
            {54, nullptr, "GetBleMtu"},
            {55, nullptr, "RegisterBleGattDataPath"},
            {56, nullptr, "UnregisterBleGattDataPath"},
            {57, nullptr, "RegisterAppletResourceUserId"},
            {58, nullptr, "UnregisterAppletResourceUserId"},
            {59, nullptr, "SetAppletResourceUserId"},
            {60, nullptr, "AcquireBleConnectionParameterUpdateEvent"}, //8.0.0+
            {61, nullptr, "SetCeLength"}, //8.0.0+
            {62, nullptr, "EnsureSlotExpansion"}, //9.0.0+
            {63, nullptr, "IsSlotExpansionEnsured"}, //9.0.0+
            {64, nullptr, "CancelConnectionTrigger"}, //10.0.0+
            {65, nullptr, "GetConnectionCapacity"}, //13.0.0+
            {66, nullptr, "GetWlanMode"}, //13.0.0+
            {67, nullptr, "IsSlotSavingEnabled"}, //13.0.0+
            {68, nullptr, "IsSlotSavingForPairingEnabled"}, //13.0.0+
            {69, nullptr, "AcquireAudioDeviceConnectionEvent"}, //13.0.0+
            {70, nullptr, "GetConnectedAudioDevices"}, //13.0.0+
            {71, nullptr, "SetAudioSourceVolume"}, //13.0.0+
            {72, nullptr, "GetAudioSourceVolume"}, //13.0.0+
            {73, nullptr, "RequestAudioDeviceConnectionRejection"}, //13.0.0+
            {74, nullptr, "CancelAudioDeviceConnectionRejection"}, //13.0.0+
            {75, nullptr, "GetPairedAudioDevices"}, //13.0.0+
            {76, nullptr, "SetWlanModeWithOption"}, //13.1.0+
            {100, nullptr, "AcquireConnectionDisallowedEvent"}, //13.0.0+
            {101, nullptr, "GetUsecaseViolationFactor"}, //13.0.0+
            {110, nullptr, "GetShortenedDeviceInfo"}, //13.0.0+
            {111, nullptr, "AcquirePairingCountUpdateEvent"},//13.0.0+
            {112, nullptr, "Unknown112"}, //14.0.0-14.1.2
            {113, nullptr, "Unknown113"}, //14.0.0-14.1.2
            {114, nullptr, "IsFirstAudioControlConnection"}, //14.0.0+
            {115, nullptr, "GetShortenedDeviceCondition"}, //14.0.0+
            {116, nullptr, "SetAudioSinkVolume"}, //15.0.0+
            {117, nullptr, "GetAudioSinkVolume"}, //15.0.0+
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void LoopProcess(Core::System& system) {
    auto server_manager = std::make_unique<ServerManager>(system);

    server_manager->RegisterNamedService("btm", std::make_shared<IBtm>(system));
    server_manager->RegisterNamedService("btm:dbg", std::make_shared<IBtmDebug>(system));
    server_manager->RegisterNamedService("btm:sys", std::make_shared<IBtmSystem>(system));
    server_manager->RegisterNamedService("btm:u", std::make_shared<IBtmUser>(system));
    ServerManager::RunServer(std::move(server_manager));
}

} // namespace Service::BTM
