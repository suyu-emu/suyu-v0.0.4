// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>

#include "core/hle/service/hid/hid_debug_server.h"
#include "core/hle/service/ipc_helpers.h"
#include "hid_core/hid_types.h"
#include "hid_core/resource_manager.h"
#include "hid_core/resources/hid_firmware_settings.h"

#include "hid_core/resources/touch_screen/gesture.h"
#include "hid_core/resources/touch_screen/touch_screen.h"
#include "hid_core/resources/touch_screen/touch_types.h"

namespace Service::HID {

IHidDebugServer::IHidDebugServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                                 std::shared_ptr<HidFirmwareSettings> settings)
    : ServiceFramework{system_, "hid:dbg"}, resource_manager{resource}, firmware_settings{
                                                                            settings} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "DeactivateDebugPad"},
        {1, nullptr, "SetDebugPadAutoPilotState"},
        {2, nullptr, "UnsetDebugPadAutoPilotState"},
        {10, &IHidDebugServer::DeactivateTouchScreen, "DeactivateTouchScreen"},
        {11, &IHidDebugServer::SetTouchScreenAutoPilotState, "SetTouchScreenAutoPilotState"},
        {12, &IHidDebugServer::UnsetTouchScreenAutoPilotState, "UnsetTouchScreenAutoPilotState"},
        {13, &IHidDebugServer::GetTouchScreenConfiguration, "GetTouchScreenConfiguration"},
        {14, &IHidDebugServer::ProcessTouchScreenAutoTune, "ProcessTouchScreenAutoTune"},
        {15, &IHidDebugServer::ForceStopTouchScreenManagement, "ForceStopTouchScreenManagement"},
        {16, &IHidDebugServer::ForceRestartTouchScreenManagement, "ForceRestartTouchScreenManagement"},
        {17, &IHidDebugServer::IsTouchScreenManaged, "IsTouchScreenManaged"},
        {20, nullptr, "DeactivateMouse"},
        {21, nullptr, "SetMouseAutoPilotState"},
        {22, nullptr, "UnsetMouseAutoPilotState"},
        {25, nullptr, "SetDebugMouseAutoPilotState"},
        {26, nullptr, "UnsetDebugMouseAutoPilotState"},
        {30, nullptr, "DeactivateKeyboard"},
        {31, nullptr, "SetKeyboardAutoPilotState"},
        {32, nullptr, "UnsetKeyboardAutoPilotState"},
        {50, nullptr, "DeactivateXpad"},
        {51, nullptr, "SetXpadAutoPilotState"},
        {52, nullptr, "UnsetXpadAutoPilotState"},
        {53, nullptr, "DeactivateJoyXpad"},
        {60, nullptr, "ClearNpadSystemCommonPolicy"},
        {61, nullptr, "DeactivateNpad"},
        {62, nullptr, "ForceDisconnectNpad"},
        {91, &IHidDebugServer::DeactivateGesture, "DeactivateGesture"},
        {110, nullptr, "DeactivateHomeButton"},
        {111, nullptr, "SetHomeButtonAutoPilotState"},
        {112, nullptr, "UnsetHomeButtonAutoPilotState"},
        {120, nullptr, "DeactivateSleepButton"},
        {121, nullptr, "SetSleepButtonAutoPilotState"},
        {122, nullptr, "UnsetSleepButtonAutoPilotState"},
        {123, nullptr, "DeactivateInputDetector"},
        {130, nullptr, "DeactivateCaptureButton"},
        {131, nullptr, "SetCaptureButtonAutoPilotState"},
        {132, nullptr, "UnsetCaptureButtonAutoPilotState"},
        {133, nullptr, "SetShiftAccelerometerCalibrationValue"},
        {134, nullptr, "GetShiftAccelerometerCalibrationValue"},
        {135, nullptr, "SetShiftGyroscopeCalibrationValue"},
        {136, nullptr, "GetShiftGyroscopeCalibrationValue"},
        {140, nullptr, "DeactivateConsoleSixAxisSensor"},
        {141, nullptr, "GetConsoleSixAxisSensorSamplingFrequency"},
        {142, nullptr, "DeactivateSevenSixAxisSensor"},
        {143, nullptr, "GetConsoleSixAxisSensorCountStates"},
        {144, nullptr, "GetAccelerometerFsr"},
        {145, nullptr, "SetAccelerometerFsr"},
        {146, nullptr, "GetAccelerometerOdr"},
        {147, nullptr, "SetAccelerometerOdr"},
        {148, nullptr, "GetGyroscopeFsr"},
        {149, nullptr, "SetGyroscopeFsr"},
        {150, nullptr, "GetGyroscopeOdr"},
        {151, nullptr, "SetGyroscopeOdr"},
        {152, nullptr, "GetWhoAmI"},
        {201, nullptr, "ActivateFirmwareUpdate"},
        {202, nullptr, "DeactivateFirmwareUpdate"},
        {203, nullptr, "StartFirmwareUpdate"},
        {204, nullptr, "GetFirmwareUpdateStage"},
        {205, nullptr, "GetFirmwareVersion"},
        {206, nullptr, "GetDestinationFirmwareVersion"},
        {207, nullptr, "DiscardFirmwareInfoCacheForRevert"},
        {208, nullptr, "StartFirmwareUpdateForRevert"},
        {209, nullptr, "GetAvailableFirmwareVersionForRevert"},
        {210, nullptr, "IsFirmwareUpdatingDevice"},
        {211, nullptr, "StartFirmwareUpdateIndividual"},
        {215, nullptr, "SetUsbFirmwareForceUpdateEnabled"},
        {216, nullptr, "SetAllKuinaDevicesToFirmwareUpdateMode"},
        {221, nullptr, "UpdateControllerColor"},
        {222, nullptr, "ConnectUsbPadsAsync"},
        {223, nullptr, "DisconnectUsbPadsAsync"},
        {224, nullptr, "UpdateDesignInfo"},
        {225, nullptr, "GetUniquePadDriverState"},
        {226, nullptr, "GetSixAxisSensorDriverStates"},
        {227, nullptr, "GetRxPacketHistory"},
        {228, nullptr, "AcquireOperationEventHandle"},
        {229, nullptr, "ReadSerialFlash"},
        {230, nullptr, "WriteSerialFlash"},
        {231, nullptr, "GetOperationResult"},
        {232, nullptr, "EnableShipmentMode"},
        {233, nullptr, "ClearPairingInfo"},
        {234, nullptr, "GetUniquePadDeviceTypeSetInternal"},
        {235, nullptr, "EnableAnalogStickPower"},
        {236, nullptr, "RequestKuinaUartClockCal"},
        {237, nullptr, "GetKuinaUartClockCal"},
        {238, nullptr, "SetKuinaUartClockTrim"},
        {239, nullptr, "KuinaLoopbackTest"},
        {240, nullptr, "RequestBatteryVoltage"},
        {241, nullptr, "GetBatteryVoltage"},
        {242, nullptr, "GetUniquePadPowerInfo"},
        {243, nullptr, "RebootUniquePad"},
        {244, nullptr, "RequestKuinaFirmwareVersion"},
        {245, nullptr, "GetKuinaFirmwareVersion"},
        {246, nullptr, "GetVidPid"},
        {247, nullptr, "GetAnalogStickCalibrationValue"},
        {248, nullptr, "GetUniquePadIdsFull"},
        {249, nullptr, "ConnectUniquePad"},
        {250, nullptr, "IsVirtual"},
        {251, nullptr, "GetAnalogStickModuleParam"},
        {301, nullptr, "GetAbstractedPadHandles"},
        {302, nullptr, "GetAbstractedPadState"},
        {303, nullptr, "GetAbstractedPadsState"},
        {321, nullptr, "SetAutoPilotVirtualPadState"},
        {322, nullptr, "UnsetAutoPilotVirtualPadState"},
        {323, nullptr, "UnsetAllAutoPilotVirtualPadState"},
        {324, nullptr, "AttachHdlsWorkBuffer"},
        {325, nullptr, "ReleaseHdlsWorkBuffer"},
        {326, nullptr, "DumpHdlsNpadAssignmentState"},
        {327, nullptr, "DumpHdlsStates"},
        {328, nullptr, "ApplyHdlsNpadAssignmentState"},
        {329, nullptr, "ApplyHdlsStateList"},
        {330, nullptr, "AttachHdlsVirtualDevice"},
        {331, nullptr, "DetachHdlsVirtualDevice"},
        {332, nullptr, "SetHdlsState"},
        {350, nullptr, "AddRegisteredDevice"},
        {400, nullptr, "DisableExternalMcuOnNxDevice"},
        {401, nullptr, "DisableRailDeviceFiltering"},
        {402, nullptr, "EnableWiredPairing"},
        {403, nullptr, "EnableShipmentModeAutoClear"},
        {404, nullptr, "SetRailEnabled"},
        {500, nullptr, "SetFactoryInt"},
        {501, nullptr, "IsFactoryBootEnabled"},
        {550, nullptr, "SetAnalogStickModelDataTemporarily"},
        {551, nullptr, "GetAnalogStickModelData"},
        {552, nullptr, "ResetAnalogStickModelData"},
        {600, nullptr, "ConvertPadState"},
        {650, nullptr, "AddButtonPlayData"},
        {651, nullptr, "StartButtonPlayData"},
        {652, nullptr, "StopButtonPlayData"},
        {2000, nullptr, "DeactivateDigitizer"},
        {2001, nullptr, "SetDigitizerAutoPilotState"},
        {2002, nullptr, "UnsetDigitizerAutoPilotState"},
        {2002, nullptr, "ReloadFirmwareDebugSettings"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IHidDebugServer::~IHidDebugServer() = default;
void IHidDebugServer::DeactivateTouchScreen(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    Result result = ResultSuccess;

    if (!firmware_settings->IsDeviceManaged()) {
        result = GetResourceManager()->GetTouchScreen()->Deactivate();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::SetTouchScreenAutoPilotState(HLERequestContext& ctx) {
    AutoPilotState auto_pilot{};
    auto_pilot.count = ctx.GetReadBufferNumElements<TouchState>();
    const auto buffer = ctx.ReadBuffer();

    auto_pilot.count = std::min(auto_pilot.count, static_cast<u64>(auto_pilot.state.size()));
    memcpy(auto_pilot.state.data(), buffer.data(), auto_pilot.count * sizeof(TouchState));

    LOG_INFO(Service_HID, "called, auto_pilot_count={}", auto_pilot.count);

    const Result result =
        GetResourceManager()->GetTouchScreen()->SetTouchScreenAutoPilotState(auto_pilot);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::UnsetTouchScreenAutoPilotState(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    const Result result = GetResourceManager()->GetTouchScreen()->UnsetTouchScreenAutoPilotState();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::GetTouchScreenConfiguration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Core::HID::TouchScreenConfigurationForNx touchscreen_config{};
    const Result result = GetResourceManager()->GetTouchScreen()->GetTouchScreenConfiguration(
        touchscreen_config, applet_resource_user_id);

    if (touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Heat2 &&
        touchscreen_config.mode != Core::HID::TouchScreenModeForNx::Finger) {
        touchscreen_config.mode = Core::HID::TouchScreenModeForNx::UseSystemSetting;
    }

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(result);
    rb.PushRaw(touchscreen_config);
}

void IHidDebugServer::ProcessTouchScreenAutoTune(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    Result result = GetResourceManager()->GetTouchScreen()->ProcessTouchScreenAutoTune();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::ForceStopTouchScreenManagement(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    if (!firmware_settings->IsDeviceManaged()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
        return;
    }

    Result result = ResultSuccess;
    bool is_touch_active{};
    bool is_gesture_active{};
    auto touch_screen = GetResourceManager()->GetTouchScreen();
    auto gesture = GetResourceManager()->GetGesture();

    if (firmware_settings->IsTouchI2cManaged()) {
        result = touch_screen->IsActive(is_touch_active);
        if (result.IsSuccess()) {
            result = gesture->IsActive(is_gesture_active);
        }
        if (result.IsSuccess() && is_touch_active) {
            result = touch_screen->Deactivate();
        }
        if (result.IsSuccess() && is_gesture_active) {
            result = gesture->Deactivate();
        }
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::ForceRestartTouchScreenManagement(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        u32 basic_gesture_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_INFO(Service_HID, "called, basic_gesture_id={}, applet_resource_user_id={}",
             parameters.basic_gesture_id, parameters.applet_resource_user_id);

    Result result = ResultSuccess;
    auto touch_screen = GetResourceManager()->GetTouchScreen();
    auto gesture = GetResourceManager()->GetGesture();

    if (firmware_settings->IsDeviceManaged() && firmware_settings->IsTouchI2cManaged()) {
        result = gesture->Activate();
        if (result.IsSuccess()) {
            result =
                gesture->Activate(parameters.applet_resource_user_id, parameters.basic_gesture_id);
        }
        if (result.IsSuccess()) {
            result = touch_screen->Activate();
        }
        if (result.IsSuccess()) {
            result = touch_screen->Activate(parameters.applet_resource_user_id);
        }
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidDebugServer::IsTouchScreenManaged(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    bool is_touch_active{};
    bool is_gesture_active{};

    Result result = GetResourceManager()->GetTouchScreen()->IsActive(is_touch_active);
    if (result.IsSuccess()) {
        result = GetResourceManager()->GetGesture()->IsActive(is_gesture_active);
    }

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(is_touch_active | is_gesture_active);
}

void IHidDebugServer::DeactivateGesture(HLERequestContext& ctx) {
    LOG_INFO(Service_HID, "called");

    Result result = ResultSuccess;

    if (!firmware_settings->IsDeviceManaged()) {
        result = GetResourceManager()->GetGesture()->Deactivate();
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

std::shared_ptr<ResourceManager> IHidDebugServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID
