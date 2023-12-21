// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/settings.h"
#include "core/hid/hid_core.h"
#include "core/hle/kernel/k_shared_memory.h"
#include "core/hle/kernel/k_transfer_memory.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/hid/errors.h"
#include "core/hle/service/hid/hid_firmware_settings.h"
#include "core/hle/service/hid/hid_server.h"
#include "core/hle/service/hid/hid_util.h"
#include "core/hle/service/hid/resource_manager.h"
#include "core/hle/service/ipc_helpers.h"
#include "core/memory.h"

#include "core/hle/service/hid/controllers/console_six_axis.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/debug_pad.h"
#include "core/hle/service/hid/controllers/gesture.h"
#include "core/hle/service/hid/controllers/keyboard.h"
#include "core/hle/service/hid/controllers/mouse.h"
#include "core/hle/service/hid/controllers/npad.h"
#include "core/hle/service/hid/controllers/palma.h"
#include "core/hle/service/hid/controllers/seven_six_axis.h"
#include "core/hle/service/hid/controllers/six_axis.h"
#include "core/hle/service/hid/controllers/touchscreen.h"
#include "core/hle/service/hid/controllers/types/npad_types.h"

namespace Service::HID {

class IActiveVibrationDeviceList final : public ServiceFramework<IActiveVibrationDeviceList> {
public:
    explicit IActiveVibrationDeviceList(Core::System& system_,
                                        std::shared_ptr<ResourceManager> resource)
        : ServiceFramework{system_, "IActiveVibrationDeviceList"}, resource_manager(resource) {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, &IActiveVibrationDeviceList::InitializeVibrationDevice, "InitializeVibrationDevice"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }

private:
    void InitializeVibrationDevice(HLERequestContext& ctx) {
        IPC::RequestParser rp{ctx};
        const auto vibration_device_handle{rp.PopRaw<Core::HID::VibrationDeviceHandle>()};

        if (resource_manager != nullptr && resource_manager->GetNpad()) {
            resource_manager->GetNpad()->InitializeVibrationDevice(vibration_device_handle);
        }

        LOG_DEBUG(Service_HID, "called, npad_type={}, npad_id={}, device_index={}",
                  vibration_device_handle.npad_type, vibration_device_handle.npad_id,
                  vibration_device_handle.device_index);

        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultSuccess);
    }

    std::shared_ptr<ResourceManager> resource_manager;
};

IHidServer::IHidServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                       std::shared_ptr<HidFirmwareSettings> settings)
    : ServiceFramework{system_, "hid"}, resource_manager{resource}, firmware_settings{settings} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, &IHidServer::CreateAppletResource, "CreateAppletResource"},
        {1, &IHidServer::ActivateDebugPad, "ActivateDebugPad"},
        {11, &IHidServer::ActivateTouchScreen, "ActivateTouchScreen"},
        {21, &IHidServer::ActivateMouse, "ActivateMouse"},
        {26, nullptr, "ActivateDebugMouse"},
        {31, &IHidServer::ActivateKeyboard, "ActivateKeyboard"},
        {32, &IHidServer::SendKeyboardLockKeyEvent, "SendKeyboardLockKeyEvent"},
        {40, &IHidServer::AcquireXpadIdEventHandle, "AcquireXpadIdEventHandle"},
        {41, &IHidServer::ReleaseXpadIdEventHandle, "ReleaseXpadIdEventHandle"},
        {51, &IHidServer::ActivateXpad, "ActivateXpad"},
        {55, &IHidServer::GetXpadIds, "GetXpadIds"},
        {56, &IHidServer::ActivateJoyXpad, "ActivateJoyXpad"},
        {58, &IHidServer::GetJoyXpadLifoHandle, "GetJoyXpadLifoHandle"},
        {59, &IHidServer::GetJoyXpadIds, "GetJoyXpadIds"},
        {60, &IHidServer::ActivateSixAxisSensor, "ActivateSixAxisSensor"},
        {61, &IHidServer::DeactivateSixAxisSensor, "DeactivateSixAxisSensor"},
        {62, &IHidServer::GetSixAxisSensorLifoHandle, "GetSixAxisSensorLifoHandle"},
        {63, &IHidServer::ActivateJoySixAxisSensor, "ActivateJoySixAxisSensor"},
        {64, &IHidServer::DeactivateJoySixAxisSensor, "DeactivateJoySixAxisSensor"},
        {65, &IHidServer::GetJoySixAxisSensorLifoHandle, "GetJoySixAxisSensorLifoHandle"},
        {66, &IHidServer::StartSixAxisSensor, "StartSixAxisSensor"},
        {67, &IHidServer::StopSixAxisSensor, "StopSixAxisSensor"},
        {68, &IHidServer::IsSixAxisSensorFusionEnabled, "IsSixAxisSensorFusionEnabled"},
        {69, &IHidServer::EnableSixAxisSensorFusion, "EnableSixAxisSensorFusion"},
        {70, &IHidServer::SetSixAxisSensorFusionParameters, "SetSixAxisSensorFusionParameters"},
        {71, &IHidServer::GetSixAxisSensorFusionParameters, "GetSixAxisSensorFusionParameters"},
        {72, &IHidServer::ResetSixAxisSensorFusionParameters, "ResetSixAxisSensorFusionParameters"},
        {73, nullptr, "SetAccelerometerParameters"},
        {74, nullptr, "GetAccelerometerParameters"},
        {75, nullptr, "ResetAccelerometerParameters"},
        {76, nullptr, "SetAccelerometerPlayMode"},
        {77, nullptr, "GetAccelerometerPlayMode"},
        {78, nullptr, "ResetAccelerometerPlayMode"},
        {79, &IHidServer::SetGyroscopeZeroDriftMode, "SetGyroscopeZeroDriftMode"},
        {80, &IHidServer::GetGyroscopeZeroDriftMode, "GetGyroscopeZeroDriftMode"},
        {81, &IHidServer::ResetGyroscopeZeroDriftMode, "ResetGyroscopeZeroDriftMode"},
        {82, &IHidServer::IsSixAxisSensorAtRest, "IsSixAxisSensorAtRest"},
        {83, &IHidServer::IsFirmwareUpdateAvailableForSixAxisSensor, "IsFirmwareUpdateAvailableForSixAxisSensor"},
        {84, &IHidServer::EnableSixAxisSensorUnalteredPassthrough, "EnableSixAxisSensorUnalteredPassthrough"},
        {85, &IHidServer::IsSixAxisSensorUnalteredPassthroughEnabled, "IsSixAxisSensorUnalteredPassthroughEnabled"},
        {86, nullptr, "StoreSixAxisSensorCalibrationParameter"},
        {87, &IHidServer::LoadSixAxisSensorCalibrationParameter, "LoadSixAxisSensorCalibrationParameter"},
        {88, &IHidServer::GetSixAxisSensorIcInformation, "GetSixAxisSensorIcInformation"},
        {89, &IHidServer::ResetIsSixAxisSensorDeviceNewlyAssigned, "ResetIsSixAxisSensorDeviceNewlyAssigned"},
        {91, &IHidServer::ActivateGesture, "ActivateGesture"},
        {100, &IHidServer::SetSupportedNpadStyleSet, "SetSupportedNpadStyleSet"},
        {101, &IHidServer::GetSupportedNpadStyleSet, "GetSupportedNpadStyleSet"},
        {102, &IHidServer::SetSupportedNpadIdType, "SetSupportedNpadIdType"},
        {103, &IHidServer::ActivateNpad, "ActivateNpad"},
        {104, &IHidServer::DeactivateNpad, "DeactivateNpad"},
        {106, &IHidServer::AcquireNpadStyleSetUpdateEventHandle, "AcquireNpadStyleSetUpdateEventHandle"},
        {107, &IHidServer::DisconnectNpad, "DisconnectNpad"},
        {108, &IHidServer::GetPlayerLedPattern, "GetPlayerLedPattern"},
        {109, &IHidServer::ActivateNpadWithRevision, "ActivateNpadWithRevision"},
        {120, &IHidServer::SetNpadJoyHoldType, "SetNpadJoyHoldType"},
        {121, &IHidServer::GetNpadJoyHoldType, "GetNpadJoyHoldType"},
        {122, &IHidServer::SetNpadJoyAssignmentModeSingleByDefault, "SetNpadJoyAssignmentModeSingleByDefault"},
        {123, &IHidServer::SetNpadJoyAssignmentModeSingle, "SetNpadJoyAssignmentModeSingle"},
        {124, &IHidServer::SetNpadJoyAssignmentModeDual, "SetNpadJoyAssignmentModeDual"},
        {125, &IHidServer::MergeSingleJoyAsDualJoy, "MergeSingleJoyAsDualJoy"},
        {126, &IHidServer::StartLrAssignmentMode, "StartLrAssignmentMode"},
        {127, &IHidServer::StopLrAssignmentMode, "StopLrAssignmentMode"},
        {128, &IHidServer::SetNpadHandheldActivationMode, "SetNpadHandheldActivationMode"},
        {129, &IHidServer::GetNpadHandheldActivationMode, "GetNpadHandheldActivationMode"},
        {130, &IHidServer::SwapNpadAssignment, "SwapNpadAssignment"},
        {131, &IHidServer::IsUnintendedHomeButtonInputProtectionEnabled, "IsUnintendedHomeButtonInputProtectionEnabled"},
        {132, &IHidServer::EnableUnintendedHomeButtonInputProtection, "EnableUnintendedHomeButtonInputProtection"},
        {133, &IHidServer::SetNpadJoyAssignmentModeSingleWithDestination, "SetNpadJoyAssignmentModeSingleWithDestination"},
        {134, &IHidServer::SetNpadAnalogStickUseCenterClamp, "SetNpadAnalogStickUseCenterClamp"},
        {135, &IHidServer::SetNpadCaptureButtonAssignment, "SetNpadCaptureButtonAssignment"},
        {136, &IHidServer::ClearNpadCaptureButtonAssignment, "ClearNpadCaptureButtonAssignment"},
        {200, &IHidServer::GetVibrationDeviceInfo, "GetVibrationDeviceInfo"},
        {201, &IHidServer::SendVibrationValue, "SendVibrationValue"},
        {202, &IHidServer::GetActualVibrationValue, "GetActualVibrationValue"},
        {203, &IHidServer::CreateActiveVibrationDeviceList, "CreateActiveVibrationDeviceList"},
        {204, &IHidServer::PermitVibration, "PermitVibration"},
        {205, &IHidServer::IsVibrationPermitted, "IsVibrationPermitted"},
        {206, &IHidServer::SendVibrationValues, "SendVibrationValues"},
        {207, &IHidServer::SendVibrationGcErmCommand, "SendVibrationGcErmCommand"},
        {208, &IHidServer::GetActualVibrationGcErmCommand, "GetActualVibrationGcErmCommand"},
        {209, &IHidServer::BeginPermitVibrationSession, "BeginPermitVibrationSession"},
        {210, &IHidServer::EndPermitVibrationSession, "EndPermitVibrationSession"},
        {211, &IHidServer::IsVibrationDeviceMounted, "IsVibrationDeviceMounted"},
        {212, nullptr, "SendVibrationValueInBool"},
        {300, &IHidServer::ActivateConsoleSixAxisSensor, "ActivateConsoleSixAxisSensor"},
        {301, &IHidServer::StartConsoleSixAxisSensor, "StartConsoleSixAxisSensor"},
        {302, &IHidServer::StopConsoleSixAxisSensor, "StopConsoleSixAxisSensor"},
        {303, &IHidServer::ActivateSevenSixAxisSensor, "ActivateSevenSixAxisSensor"},
        {304, &IHidServer::StartSevenSixAxisSensor, "StartSevenSixAxisSensor"},
        {305, &IHidServer::StopSevenSixAxisSensor, "StopSevenSixAxisSensor"},
        {306, &IHidServer::InitializeSevenSixAxisSensor, "InitializeSevenSixAxisSensor"},
        {307, &IHidServer::FinalizeSevenSixAxisSensor, "FinalizeSevenSixAxisSensor"},
        {308, nullptr, "SetSevenSixAxisSensorFusionStrength"},
        {309, nullptr, "GetSevenSixAxisSensorFusionStrength"},
        {310, &IHidServer::ResetSevenSixAxisSensorTimestamp, "ResetSevenSixAxisSensorTimestamp"},
        {400, &IHidServer::IsUsbFullKeyControllerEnabled, "IsUsbFullKeyControllerEnabled"},
        {401, nullptr, "EnableUsbFullKeyController"},
        {402, nullptr, "IsUsbFullKeyControllerConnected"},
        {403, nullptr, "HasBattery"},
        {404, nullptr, "HasLeftRightBattery"},
        {405, nullptr, "GetNpadInterfaceType"},
        {406, nullptr, "GetNpadLeftRightInterfaceType"},
        {407, nullptr, "GetNpadOfHighestBatteryLevel"},
        {408, nullptr, "GetNpadOfHighestBatteryLevelForJoyRight"},
        {500, &IHidServer::GetPalmaConnectionHandle, "GetPalmaConnectionHandle"},
        {501, &IHidServer::InitializePalma, "InitializePalma"},
        {502, &IHidServer::AcquirePalmaOperationCompleteEvent, "AcquirePalmaOperationCompleteEvent"},
        {503, &IHidServer::GetPalmaOperationInfo, "GetPalmaOperationInfo"},
        {504, &IHidServer::PlayPalmaActivity, "PlayPalmaActivity"},
        {505, &IHidServer::SetPalmaFrModeType, "SetPalmaFrModeType"},
        {506, &IHidServer::ReadPalmaStep, "ReadPalmaStep"},
        {507, &IHidServer::EnablePalmaStep, "EnablePalmaStep"},
        {508, &IHidServer::ResetPalmaStep, "ResetPalmaStep"},
        {509, &IHidServer::ReadPalmaApplicationSection, "ReadPalmaApplicationSection"},
        {510, &IHidServer::WritePalmaApplicationSection, "WritePalmaApplicationSection"},
        {511, &IHidServer::ReadPalmaUniqueCode, "ReadPalmaUniqueCode"},
        {512, &IHidServer::SetPalmaUniqueCodeInvalid, "SetPalmaUniqueCodeInvalid"},
        {513, &IHidServer::WritePalmaActivityEntry, "WritePalmaActivityEntry"},
        {514, &IHidServer::WritePalmaRgbLedPatternEntry, "WritePalmaRgbLedPatternEntry"},
        {515, &IHidServer::WritePalmaWaveEntry, "WritePalmaWaveEntry"},
        {516, &IHidServer::SetPalmaDataBaseIdentificationVersion, "SetPalmaDataBaseIdentificationVersion"},
        {517, &IHidServer::GetPalmaDataBaseIdentificationVersion, "GetPalmaDataBaseIdentificationVersion"},
        {518, &IHidServer::SuspendPalmaFeature, "SuspendPalmaFeature"},
        {519, &IHidServer::GetPalmaOperationResult, "GetPalmaOperationResult"},
        {520, &IHidServer::ReadPalmaPlayLog, "ReadPalmaPlayLog"},
        {521, &IHidServer::ResetPalmaPlayLog, "ResetPalmaPlayLog"},
        {522, &IHidServer::SetIsPalmaAllConnectable, "SetIsPalmaAllConnectable"},
        {523, &IHidServer::SetIsPalmaPairedConnectable, "SetIsPalmaPairedConnectable"},
        {524, &IHidServer::PairPalma, "PairPalma"},
        {525, &IHidServer::SetPalmaBoostMode, "SetPalmaBoostMode"},
        {526, &IHidServer::CancelWritePalmaWaveEntry, "CancelWritePalmaWaveEntry"},
        {527, &IHidServer::EnablePalmaBoostMode, "EnablePalmaBoostMode"},
        {528, &IHidServer::GetPalmaBluetoothAddress, "GetPalmaBluetoothAddress"},
        {529, &IHidServer::SetDisallowedPalmaConnection, "SetDisallowedPalmaConnection"},
        {1000, &IHidServer::SetNpadCommunicationMode, "SetNpadCommunicationMode"},
        {1001, &IHidServer::GetNpadCommunicationMode, "GetNpadCommunicationMode"},
        {1002, &IHidServer::SetTouchScreenConfiguration, "SetTouchScreenConfiguration"},
        {1003, &IHidServer::IsFirmwareUpdateNeededForNotification, "IsFirmwareUpdateNeededForNotification"},
        {1004, &IHidServer::SetTouchScreenResolution, "SetTouchScreenResolution"},
        {2000, nullptr, "ActivateDigitizer"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IHidServer::~IHidServer() = default;

void IHidServer::CreateAppletResource(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    Result result = GetResourceManager()->CreateAppletResource(applet_resource_user_id);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, result=0x{:X}",
              applet_resource_user_id, result.raw);

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(result);
    rb.PushIpcInterface<IAppletResource>(system, resource_manager, applet_resource_user_id);
}

void IHidServer::ActivateDebugPad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto debug_pad = GetResourceManager()->GetDebugPad();

    if (!firmware_settings->IsDeviceManaged()) {
        result = debug_pad->Activate();
    }

    if (result.IsSuccess()) {
        result = debug_pad->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ActivateTouchScreen(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto touch_screen = GetResourceManager()->GetTouchScreen();

    if (!firmware_settings->IsDeviceManaged()) {
        result = touch_screen->Activate();
    }

    if (result.IsSuccess()) {
        result = touch_screen->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ActivateMouse(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto mouse = GetResourceManager()->GetMouse();

    if (!firmware_settings->IsDeviceManaged()) {
        result = mouse->Activate();
    }

    if (result.IsSuccess()) {
        result = mouse->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ActivateKeyboard(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto keyboard = GetResourceManager()->GetKeyboard();

    if (!firmware_settings->IsDeviceManaged()) {
        result = keyboard->Activate();
    }

    if (result.IsSuccess()) {
        result = keyboard->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SendKeyboardLockKeyEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto flags{rp.Pop<u32>()};

    LOG_WARNING(Service_HID, "(STUBBED) called. flags={}", flags);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::AcquireXpadIdEventHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    // Handle returned is null here
}

void IHidServer::ReleaseXpadIdEventHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ActivateXpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        u32 basic_xpad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_DEBUG(Service_HID, "called, basic_xpad_id={}, applet_resource_user_id={}",
              parameters.basic_xpad_id, parameters.applet_resource_user_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetXpadIds(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    // This function has been hardcoded since 10.0.0+
    const std::array<u32, 4> basic_xpad_id{0, 1, 2, 3};
    ctx.WriteBuffer(basic_xpad_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<s64>(basic_xpad_id.size());
}

void IHidServer::ActivateJoyXpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetJoyXpadLifoHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    // Handle returned is null here
}

void IHidServer::GetJoyXpadIds(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    // This function has been hardcoded since 10.0.0+
    const s64 basic_xpad_id_count{};

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(basic_xpad_id_count);
}

void IHidServer::ActivateSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::DeactivateSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
}

void IHidServer::GetSixAxisSensorLifoHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ActivateJoySixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::DeactivateJoySixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetJoySixAxisSensorLifoHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto joy_xpad_id{rp.Pop<u32>()};

    LOG_DEBUG(Service_HID, "called, joy_xpad_id={}", joy_xpad_id);

    // This function has been stubbed since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    // Handle returned is null here
}

void IHidServer::StartSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->SetSixAxisEnabled(parameters.sixaxis_handle, true);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::StopSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->SetSixAxisEnabled(parameters.sixaxis_handle, false);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::IsSixAxisSensorFusionEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    bool is_enabled{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result =
        six_axis->IsSixAxisSensorFusionEnabled(parameters.sixaxis_handle, is_enabled);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(is_enabled);
}

void IHidServer::EnableSixAxisSensorFusion(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool enable_sixaxis_sensor_fusion;
        INSERT_PADDING_BYTES_NOINIT(3);
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->SetSixAxisFusionEnabled(parameters.sixaxis_handle,
                                                          parameters.enable_sixaxis_sensor_fusion);

    LOG_DEBUG(Service_HID,
              "called, enable_sixaxis_sensor_fusion={}, npad_type={}, npad_id={}, "
              "device_index={}, applet_resource_user_id={}",
              parameters.enable_sixaxis_sensor_fusion, parameters.sixaxis_handle.npad_type,
              parameters.sixaxis_handle.npad_id, parameters.sixaxis_handle.device_index,
              parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SetSixAxisSensorFusionParameters(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        Core::HID::SixAxisSensorFusionParameters sixaxis_fusion;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result =
        six_axis->SetSixAxisFusionParameters(parameters.sixaxis_handle, parameters.sixaxis_fusion);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, parameter1={}, "
              "parameter2={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.sixaxis_fusion.parameter1,
              parameters.sixaxis_fusion.parameter2, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::GetSixAxisSensorFusionParameters(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::SixAxisSensorFusionParameters fusion_parameters{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result =
        six_axis->GetSixAxisFusionParameters(parameters.sixaxis_handle, fusion_parameters);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.PushRaw(fusion_parameters);
}

void IHidServer::ResetSixAxisSensorFusionParameters(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    // Since these parameters are unknown just use what HW outputs
    const Core::HID::SixAxisSensorFusionParameters fusion_parameters{
        .parameter1 = 0.03f,
        .parameter2 = 0.4f,
    };
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result1 =
        six_axis->SetSixAxisFusionParameters(parameters.sixaxis_handle, fusion_parameters);
    const auto result2 = six_axis->SetSixAxisFusionEnabled(parameters.sixaxis_handle, true);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    if (result1.IsError()) {
        rb.Push(result1);
        return;
    }
    rb.Push(result2);
}

void IHidServer::SetGyroscopeZeroDriftMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto sixaxis_handle{rp.PopRaw<Core::HID::SixAxisSensorHandle>()};
    const auto drift_mode{rp.PopEnum<Core::HID::GyroscopeZeroDriftMode>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->SetGyroscopeZeroDriftMode(sixaxis_handle, drift_mode);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, drift_mode={}, "
              "applet_resource_user_id={}",
              sixaxis_handle.npad_type, sixaxis_handle.npad_id, sixaxis_handle.device_index,
              drift_mode, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::GetGyroscopeZeroDriftMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto drift_mode{Core::HID::GyroscopeZeroDriftMode::Standard};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->GetGyroscopeZeroDriftMode(parameters.sixaxis_handle, drift_mode);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.PushEnum(drift_mode);
}

void IHidServer::ResetGyroscopeZeroDriftMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    const auto drift_mode{Core::HID::GyroscopeZeroDriftMode::Standard};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->SetGyroscopeZeroDriftMode(parameters.sixaxis_handle, drift_mode);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::IsSixAxisSensorAtRest(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    bool is_at_rest{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    six_axis->IsSixAxisSensorAtRest(parameters.sixaxis_handle, is_at_rest);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
              parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_at_rest);
}

void IHidServer::IsFirmwareUpdateAvailableForSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    bool is_firmware_available{};
    auto controller = GetResourceManager()->GetNpad();
    controller->IsFirmwareUpdateAvailableForSixAxisSensor(parameters.sixaxis_handle,
                                                          is_firmware_available);

    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
        parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_firmware_available);
}

void IHidServer::EnableSixAxisSensorUnalteredPassthrough(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool enabled;
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->EnableSixAxisSensorUnalteredPassthrough(parameters.sixaxis_handle,
                                                                          parameters.enabled);

    LOG_DEBUG(Service_HID,
              "(STUBBED) called, enabled={}, npad_type={}, npad_id={}, device_index={}, "
              "applet_resource_user_id={}",
              parameters.enabled, parameters.sixaxis_handle.npad_type,
              parameters.sixaxis_handle.npad_id, parameters.sixaxis_handle.device_index,
              parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::IsSixAxisSensorUnalteredPassthroughEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    bool is_unaltered_sisxaxis_enabled{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result = six_axis->IsSixAxisSensorUnalteredPassthroughEnabled(
        parameters.sixaxis_handle, is_unaltered_sisxaxis_enabled);

    LOG_DEBUG(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
        parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(is_unaltered_sisxaxis_enabled);
}

void IHidServer::LoadSixAxisSensorCalibrationParameter(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::SixAxisSensorCalibrationParameter calibration{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result =
        six_axis->LoadSixAxisSensorCalibrationParameter(parameters.sixaxis_handle, calibration);

    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
        parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(calibration);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::GetSixAxisSensorIcInformation(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::SixAxisSensorIcInformation ic_information{};
    auto six_axis = GetResourceManager()->GetSixAxis();
    const auto result =
        six_axis->GetSixAxisSensorIcInformation(parameters.sixaxis_handle, ic_information);

    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
        parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    if (result.IsSuccess()) {
        ctx.WriteBuffer(ic_information);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ResetIsSixAxisSensorDeviceNewlyAssigned(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::SixAxisSensorHandle sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto controller = GetResourceManager()->GetNpad();
    const auto result =
        controller->ResetIsSixAxisSensorDeviceNewlyAssigned(parameters.sixaxis_handle);

    LOG_WARNING(
        Service_HID,
        "(STUBBED) called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
        parameters.sixaxis_handle.npad_type, parameters.sixaxis_handle.npad_id,
        parameters.sixaxis_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ActivateGesture(HLERequestContext& ctx) {
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
    auto gesture = GetResourceManager()->GetGesture();

    if (!firmware_settings->IsDeviceManaged()) {
        result = gesture->Activate();
    }

    if (result.IsSuccess()) {
        // TODO: Use gesture id here
        result = gesture->Activate(parameters.applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SetSupportedNpadStyleSet(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadStyleSet supported_styleset;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    GetResourceManager()->GetNpad()->SetSupportedStyleSet({parameters.supported_styleset});

    LOG_DEBUG(Service_HID, "called, supported_styleset={}, applet_resource_user_id={}",
              parameters.supported_styleset, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetSupportedNpadStyleSet(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.PushEnum(GetResourceManager()->GetNpad()->GetSupportedStyleSet().raw);
}

void IHidServer::SetSupportedNpadIdType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    const auto result = GetResourceManager()->GetNpad()->SetSupportedNpadIdTypes(ctx.ReadBuffer());

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ActivateNpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    auto npad = GetResourceManager()->GetNpad();

    // TODO: npad->SetRevision(applet_resource_user_id, NpadRevision::Revision0);
    const Result result = npad->Activate(applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::DeactivateNpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    // This function does nothing since 10.0.0+

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::AcquireNpadStyleSetUpdateEventHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
        u64 unknown;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}, unknown={}",
              parameters.npad_id, parameters.applet_resource_user_id, parameters.unknown);

    // Games expect this event to be signaled after calling this function
    GetResourceManager()->GetNpad()->SignalStyleSetChangedEvent(parameters.npad_id);

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(
        GetResourceManager()->GetNpad()->GetStyleSetChangedEvent(parameters.npad_id));
}

void IHidServer::DisconnectNpad(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto controller = GetResourceManager()->GetNpad();
    controller->DisconnectNpad(parameters.npad_id);

    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}", parameters.npad_id,
              parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetPlayerLedPattern(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id{rp.PopEnum<Core::HID::NpadIdType>()};

    Core::HID::LedPattern pattern{0, 0, 0, 0};
    auto controller = GetResourceManager()->GetNpad();
    const auto result = controller->GetLedPattern(npad_id, pattern);

    LOG_DEBUG(Service_HID, "called, npad_id={}", npad_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(pattern.raw);
}

void IHidServer::ActivateNpadWithRevision(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        NpadRevision revision;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_DEBUG(Service_HID, "called, revision={}, applet_resource_user_id={}", parameters.revision,
              parameters.applet_resource_user_id);

    auto npad = GetResourceManager()->GetNpad();

    // TODO: npad->SetRevision(applet_resource_user_id, revision);
    const auto result = npad->Activate(parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SetNpadJoyHoldType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};
    const auto hold_type{rp.PopEnum<NpadJoyHoldType>()};

    GetResourceManager()->GetNpad()->SetHoldType(hold_type);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, hold_type={}",
              applet_resource_user_id, hold_type);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetNpadJoyHoldType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(GetResourceManager()->GetNpad()->GetHoldType());
}

void IHidServer::SetNpadJoyAssignmentModeSingleByDefault(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::NpadIdType new_npad_id{};
    auto controller = GetResourceManager()->GetNpad();
    controller->SetNpadMode(new_npad_id, parameters.npad_id, NpadJoyDeviceType::Left,
                            NpadJoyAssignmentMode::Single);

    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}", parameters.npad_id,
             parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetNpadJoyAssignmentModeSingle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
        NpadJoyDeviceType npad_joy_device_type;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::NpadIdType new_npad_id{};
    auto controller = GetResourceManager()->GetNpad();
    controller->SetNpadMode(new_npad_id, parameters.npad_id, parameters.npad_joy_device_type,
                            NpadJoyAssignmentMode::Single);

    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}, npad_joy_device_type={}",
             parameters.npad_id, parameters.applet_resource_user_id,
             parameters.npad_joy_device_type);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetNpadJoyAssignmentModeDual(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::NpadIdType new_npad_id{};
    auto controller = GetResourceManager()->GetNpad();
    controller->SetNpadMode(new_npad_id, parameters.npad_id, {}, NpadJoyAssignmentMode::Dual);

    LOG_DEBUG(Service_HID, "called, npad_id={}, applet_resource_user_id={}", parameters.npad_id,
              parameters.applet_resource_user_id); // Spams a lot when controller applet is open

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::MergeSingleJoyAsDualJoy(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_1{rp.PopEnum<Core::HID::NpadIdType>()};
    const auto npad_id_2{rp.PopEnum<Core::HID::NpadIdType>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    auto controller = GetResourceManager()->GetNpad();
    const auto result = controller->MergeSingleJoyAsDualJoy(npad_id_1, npad_id_2);

    LOG_DEBUG(Service_HID, "called, npad_id_1={}, npad_id_2={}, applet_resource_user_id={}",
              npad_id_1, npad_id_2, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::StartLrAssignmentMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    GetResourceManager()->GetNpad()->StartLRAssignmentMode();

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::StopLrAssignmentMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    GetResourceManager()->GetNpad()->StopLRAssignmentMode();

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetNpadHandheldActivationMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};
    const auto activation_mode{rp.PopEnum<NpadHandheldActivationMode>()};

    GetResourceManager()->GetNpad()->SetNpadHandheldActivationMode(activation_mode);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}, activation_mode={}",
              applet_resource_user_id, activation_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetNpadHandheldActivationMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(GetResourceManager()->GetNpad()->GetNpadHandheldActivationMode());
}

void IHidServer::SwapNpadAssignment(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto npad_id_1{rp.PopEnum<Core::HID::NpadIdType>()};
    const auto npad_id_2{rp.PopEnum<Core::HID::NpadIdType>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    auto controller = GetResourceManager()->GetNpad();
    const auto result = controller->SwapNpadAssignment(npad_id_1, npad_id_2);

    LOG_DEBUG(Service_HID, "called, npad_id_1={}, npad_id_2={}, applet_resource_user_id={}",
              npad_id_1, npad_id_2, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::IsUnintendedHomeButtonInputProtectionEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    bool is_enabled = false;
    auto controller = GetResourceManager()->GetNpad();
    const auto result =
        controller->IsUnintendedHomeButtonInputProtectionEnabled(parameters.npad_id, is_enabled);

    LOG_WARNING(Service_HID, "(STUBBED) called, npad_id={}, applet_resource_user_id={}",
                parameters.npad_id, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(result);
    rb.Push(is_enabled);
}

void IHidServer::EnableUnintendedHomeButtonInputProtection(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_BYTES_NOINIT(3);
        Core::HID::NpadIdType npad_id;
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    auto controller = GetResourceManager()->GetNpad();
    const auto result = controller->SetUnintendedHomeButtonInputProtectionEnabled(
        parameters.is_enabled, parameters.npad_id);

    LOG_DEBUG(Service_HID,
              "(STUBBED) called, is_enabled={}, npad_id={}, applet_resource_user_id={}",
              parameters.is_enabled, parameters.npad_id, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SetNpadJoyAssignmentModeSingleWithDestination(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
        NpadJoyDeviceType npad_joy_device_type;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    Core::HID::NpadIdType new_npad_id{};
    auto controller = GetResourceManager()->GetNpad();
    const auto is_reassigned =
        controller->SetNpadMode(new_npad_id, parameters.npad_id, parameters.npad_joy_device_type,
                                NpadJoyAssignmentMode::Single);

    LOG_INFO(Service_HID, "called, npad_id={}, applet_resource_user_id={}, npad_joy_device_type={}",
             parameters.npad_id, parameters.applet_resource_user_id,
             parameters.npad_joy_device_type);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push(is_reassigned);
    rb.PushEnum(new_npad_id);
}

void IHidServer::SetNpadAnalogStickUseCenterClamp(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool analog_stick_use_center_clamp;
        INSERT_PADDING_BYTES_NOINIT(7);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    GetResourceManager()->GetNpad()->SetAnalogStickUseCenterClamp(
        parameters.analog_stick_use_center_clamp);

    LOG_WARNING(Service_HID,
                "(STUBBED) called, analog_stick_use_center_clamp={}, applet_resource_user_id={}",
                parameters.analog_stick_use_center_clamp, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetNpadCaptureButtonAssignment(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadStyleSet npad_styleset;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
        Core::HID::NpadButton button;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID,
                "(STUBBED) called, npad_styleset={}, applet_resource_user_id={}, button={}",
                parameters.npad_styleset, parameters.applet_resource_user_id, parameters.button);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ClearNpadCaptureButtonAssignment(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}",
                applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetVibrationDeviceInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto vibration_device_handle{rp.PopRaw<Core::HID::VibrationDeviceHandle>()};
    const auto controller = GetResourceManager()->GetNpad();

    Core::HID::VibrationDeviceInfo vibration_device_info;
    bool check_device_index = false;

    switch (vibration_device_handle.npad_type) {
    case Core::HID::NpadStyleIndex::ProController:
    case Core::HID::NpadStyleIndex::Handheld:
    case Core::HID::NpadStyleIndex::JoyconDual:
    case Core::HID::NpadStyleIndex::JoyconLeft:
    case Core::HID::NpadStyleIndex::JoyconRight:
        vibration_device_info.type = Core::HID::VibrationDeviceType::LinearResonantActuator;
        check_device_index = true;
        break;
    case Core::HID::NpadStyleIndex::GameCube:
        vibration_device_info.type = Core::HID::VibrationDeviceType::GcErm;
        break;
    case Core::HID::NpadStyleIndex::N64:
        vibration_device_info.type = Core::HID::VibrationDeviceType::N64;
        break;
    default:
        vibration_device_info.type = Core::HID::VibrationDeviceType::Unknown;
        break;
    }

    vibration_device_info.position = Core::HID::VibrationDevicePosition::None;
    if (check_device_index) {
        switch (vibration_device_handle.device_index) {
        case Core::HID::DeviceIndex::Left:
            vibration_device_info.position = Core::HID::VibrationDevicePosition::Left;
            break;
        case Core::HID::DeviceIndex::Right:
            vibration_device_info.position = Core::HID::VibrationDevicePosition::Right;
            break;
        case Core::HID::DeviceIndex::None:
        default:
            ASSERT_MSG(false, "DeviceIndex should never be None!");
            break;
        }
    }

    LOG_DEBUG(Service_HID, "called, vibration_device_type={}, vibration_device_position={}",
              vibration_device_info.type, vibration_device_info.position);

    const auto result = IsVibrationHandleValid(vibration_device_handle);
    if (result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
        return;
    }

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushRaw(vibration_device_info);
}

void IHidServer::SendVibrationValue(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::VibrationDeviceHandle vibration_device_handle;
        Core::HID::VibrationValue vibration_value;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x20, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    GetResourceManager()->GetNpad()->VibrateController(parameters.vibration_device_handle,
                                                       parameters.vibration_value);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.vibration_device_handle.npad_type,
              parameters.vibration_device_handle.npad_id,
              parameters.vibration_device_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetActualVibrationValue(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::VibrationDeviceHandle vibration_device_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.vibration_device_handle.npad_type,
              parameters.vibration_device_handle.npad_id,
              parameters.vibration_device_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 6};
    rb.Push(ResultSuccess);
    rb.PushRaw(
        GetResourceManager()->GetNpad()->GetLastVibration(parameters.vibration_device_handle));
}

void IHidServer::CreateActiveVibrationDeviceList(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    IPC::ResponseBuilder rb{ctx, 2, 0, 1};
    rb.Push(ResultSuccess);
    rb.PushIpcInterface<IActiveVibrationDeviceList>(system, GetResourceManager());
}

void IHidServer::PermitVibration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto can_vibrate{rp.Pop<bool>()};

    // nnSDK saves this value as a float. Since it can only be 1.0f or 0.0f we simplify this value
    // by converting it to a bool
    Settings::values.vibration_enabled.SetValue(can_vibrate);

    LOG_DEBUG(Service_HID, "called, can_vibrate={}", can_vibrate);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::IsVibrationPermitted(HLERequestContext& ctx) {
    LOG_DEBUG(Service_HID, "called");

    // nnSDK checks if a float is greater than zero. We return the bool we stored earlier
    const auto is_enabled = Settings::values.vibration_enabled.GetValue();

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(is_enabled);
}

void IHidServer::SendVibrationValues(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    const auto handle_data = ctx.ReadBuffer(0);
    const auto handle_count = ctx.GetReadBufferNumElements<Core::HID::VibrationDeviceHandle>(0);
    const auto vibration_data = ctx.ReadBuffer(1);
    const auto vibration_count = ctx.GetReadBufferNumElements<Core::HID::VibrationValue>(1);

    auto vibration_device_handles =
        std::span(reinterpret_cast<const Core::HID::VibrationDeviceHandle*>(handle_data.data()),
                  handle_count);
    auto vibration_values = std::span(
        reinterpret_cast<const Core::HID::VibrationValue*>(vibration_data.data()), vibration_count);

    GetResourceManager()->GetNpad()->VibrateControllers(vibration_device_handles, vibration_values);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SendVibrationGcErmCommand(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::VibrationDeviceHandle vibration_device_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
        Core::HID::VibrationGcErmCommand gc_erm_command;
    };
    static_assert(sizeof(Parameters) == 0x18, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    /**
     * Note: This uses yuzu-specific behavior such that the StopHard command produces
     * vibrations where freq_low == 0.0f and freq_high == 0.0f, as defined below,
     * in order to differentiate between Stop and StopHard commands.
     * This is done to reuse the controller vibration functions made for regular controllers.
     */
    const auto vibration_value = [parameters] {
        switch (parameters.gc_erm_command) {
        case Core::HID::VibrationGcErmCommand::Stop:
            return Core::HID::VibrationValue{
                .low_amplitude = 0.0f,
                .low_frequency = 160.0f,
                .high_amplitude = 0.0f,
                .high_frequency = 320.0f,
            };
        case Core::HID::VibrationGcErmCommand::Start:
            return Core::HID::VibrationValue{
                .low_amplitude = 1.0f,
                .low_frequency = 160.0f,
                .high_amplitude = 1.0f,
                .high_frequency = 320.0f,
            };
        case Core::HID::VibrationGcErmCommand::StopHard:
            return Core::HID::VibrationValue{
                .low_amplitude = 0.0f,
                .low_frequency = 0.0f,
                .high_amplitude = 0.0f,
                .high_frequency = 0.0f,
            };
        default:
            return Core::HID::DEFAULT_VIBRATION_VALUE;
        }
    }();

    GetResourceManager()->GetNpad()->VibrateController(parameters.vibration_device_handle,
                                                       vibration_value);

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}, "
              "gc_erm_command={}",
              parameters.vibration_device_handle.npad_type,
              parameters.vibration_device_handle.npad_id,
              parameters.vibration_device_handle.device_index, parameters.applet_resource_user_id,
              parameters.gc_erm_command);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetActualVibrationGcErmCommand(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::VibrationDeviceHandle vibration_device_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };

    const auto parameters{rp.PopRaw<Parameters>()};

    const auto last_vibration =
        GetResourceManager()->GetNpad()->GetLastVibration(parameters.vibration_device_handle);

    const auto gc_erm_command = [last_vibration] {
        if (last_vibration.low_amplitude != 0.0f || last_vibration.high_amplitude != 0.0f) {
            return Core::HID::VibrationGcErmCommand::Start;
        }

        /**
         * Note: This uses yuzu-specific behavior such that the StopHard command produces
         * vibrations where freq_low == 0.0f and freq_high == 0.0f, as defined in the HID function
         * SendVibrationGcErmCommand, in order to differentiate between Stop and StopHard commands.
         * This is done to reuse the controller vibration functions made for regular controllers.
         */
        if (last_vibration.low_frequency == 0.0f && last_vibration.high_frequency == 0.0f) {
            return Core::HID::VibrationGcErmCommand::StopHard;
        }

        return Core::HID::VibrationGcErmCommand::Stop;
    }();

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.vibration_device_handle.npad_type,
              parameters.vibration_device_handle.npad_id,
              parameters.vibration_device_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(gc_erm_command);
}

void IHidServer::BeginPermitVibrationSession(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    GetResourceManager()->GetNpad()->SetPermitVibrationSession(true);

    LOG_DEBUG(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::EndPermitVibrationSession(HLERequestContext& ctx) {
    GetResourceManager()->GetNpad()->SetPermitVibrationSession(false);

    LOG_DEBUG(Service_HID, "called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::IsVibrationDeviceMounted(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::VibrationDeviceHandle vibration_device_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_DEBUG(Service_HID,
              "called, npad_type={}, npad_id={}, device_index={}, applet_resource_user_id={}",
              parameters.vibration_device_handle.npad_type,
              parameters.vibration_device_handle.npad_id,
              parameters.vibration_device_handle.device_index, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(GetResourceManager()->GetNpad()->IsVibrationDeviceMounted(
        parameters.vibration_device_handle));
}

void IHidServer::ActivateConsoleSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto console_sixaxis = GetResourceManager()->GetConsoleSixAxis();

    if (!firmware_settings->IsDeviceManaged()) {
        result = console_sixaxis->Activate();
    }

    if (result.IsSuccess()) {
        result = console_sixaxis->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::StartConsoleSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::ConsoleSixAxisSensorHandle console_sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID,
                "(STUBBED) called, unknown_1={}, unknown_2={}, applet_resource_user_id={}",
                parameters.console_sixaxis_handle.unknown_1,
                parameters.console_sixaxis_handle.unknown_2, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::StopConsoleSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::ConsoleSixAxisSensorHandle console_sixaxis_handle;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID,
                "(STUBBED) called, unknown_1={}, unknown_2={}, applet_resource_user_id={}",
                parameters.console_sixaxis_handle.unknown_1,
                parameters.console_sixaxis_handle.unknown_2, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ActivateSevenSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_INFO(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    Result result = ResultSuccess;
    auto seven_sixaxis = GetResourceManager()->GetSevenSixAxis();

    if (!firmware_settings->IsDeviceManaged()) {
        result = seven_sixaxis->Activate();
    }

    if (result.IsSuccess()) {
        seven_sixaxis->Activate(applet_resource_user_id);
    }

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::StartSevenSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}",
                applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::StopSevenSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}",
                applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::InitializeSevenSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};
    const auto t_mem_1_size{rp.Pop<u64>()};
    const auto t_mem_2_size{rp.Pop<u64>()};
    const auto t_mem_1_handle{ctx.GetCopyHandle(0)};
    const auto t_mem_2_handle{ctx.GetCopyHandle(1)};

    ASSERT_MSG(t_mem_1_size == 0x1000, "t_mem_1_size is not 0x1000 bytes");
    ASSERT_MSG(t_mem_2_size == 0x7F000, "t_mem_2_size is not 0x7F000 bytes");

    auto t_mem_1 = system.ApplicationProcess()->GetHandleTable().GetObject<Kernel::KTransferMemory>(
        t_mem_1_handle);

    if (t_mem_1.IsNull()) {
        LOG_ERROR(Service_HID, "t_mem_1 is a nullptr for handle=0x{:08X}", t_mem_1_handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    auto t_mem_2 = system.ApplicationProcess()->GetHandleTable().GetObject<Kernel::KTransferMemory>(
        t_mem_2_handle);

    if (t_mem_2.IsNull()) {
        LOG_ERROR(Service_HID, "t_mem_2 is a nullptr for handle=0x{:08X}", t_mem_2_handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    ASSERT_MSG(t_mem_1->GetSize() == 0x1000, "t_mem_1 has incorrect size");
    ASSERT_MSG(t_mem_2->GetSize() == 0x7F000, "t_mem_2 has incorrect size");

    // Activate console six axis controller
    GetResourceManager()->GetConsoleSixAxis()->Activate();
    GetResourceManager()->GetSevenSixAxis()->Activate();

    GetResourceManager()->GetSevenSixAxis()->SetTransferMemoryAddress(t_mem_1->GetSourceAddress());

    LOG_WARNING(Service_HID,
                "called, t_mem_1_handle=0x{:08X}, t_mem_2_handle=0x{:08X}, "
                "applet_resource_user_id={}",
                t_mem_1_handle, t_mem_2_handle, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::FinalizeSevenSixAxisSensor(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}",
                applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ResetSevenSixAxisSensorTimestamp(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    GetResourceManager()->GetSevenSixAxis()->ResetTimestamp();

    LOG_WARNING(Service_HID, "called, applet_resource_user_id={}", applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::IsUsbFullKeyControllerEnabled(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(false);
}

void IHidServer::GetPalmaConnectionHandle(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        Core::HID::NpadIdType npad_id;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, npad_id={}, applet_resource_user_id={}",
                parameters.npad_id, parameters.applet_resource_user_id);

    Palma::PalmaConnectionHandle handle;
    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->GetPalmaConnectionHandle(parameters.npad_id, handle);

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.PushRaw(handle);
}

void IHidServer::InitializePalma(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->InitializePalma(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::AcquirePalmaOperationCompleteEvent(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    auto controller = GetResourceManager()->GetPalma();

    IPC::ResponseBuilder rb{ctx, 2, 1};
    rb.Push(ResultSuccess);
    rb.PushCopyObjects(controller->AcquirePalmaOperationCompleteEvent(connection_handle));
}

void IHidServer::GetPalmaOperationInfo(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    Palma::PalmaOperationType operation_type;
    Palma::PalmaOperationData data;
    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->GetPalmaOperationInfo(connection_handle, operation_type, data);

    if (result.IsError()) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(result);
    }

    ctx.WriteBuffer(data);
    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(result);
    rb.Push(static_cast<u64>(operation_type));
}

void IHidServer::PlayPalmaActivity(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};
    const auto palma_activity{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, palma_activity={}",
                connection_handle.npad_id, palma_activity);

    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->PlayPalmaActivity(connection_handle, palma_activity);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::SetPalmaFrModeType(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};
    const auto fr_mode{rp.PopEnum<Palma::PalmaFrModeType>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, fr_mode={}",
                connection_handle.npad_id, fr_mode);

    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->SetPalmaFrModeType(connection_handle, fr_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ReadPalmaStep(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->ReadPalmaStep(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::EnablePalmaStep(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_enabled;
        INSERT_PADDING_WORDS_NOINIT(1);
        Palma::PalmaConnectionHandle connection_handle;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, is_enabled={}",
                parameters.connection_handle.npad_id, parameters.is_enabled);

    auto controller = GetResourceManager()->GetPalma();
    const auto result =
        controller->EnablePalmaStep(parameters.connection_handle, parameters.is_enabled);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ResetPalmaStep(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    auto controller = GetResourceManager()->GetPalma();
    const auto result = controller->ResetPalmaStep(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ReadPalmaApplicationSection(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::WritePalmaApplicationSection(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ReadPalmaUniqueCode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->ReadPalmaUniqueCode(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetPalmaUniqueCodeInvalid(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->SetPalmaUniqueCodeInvalid(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::WritePalmaActivityEntry(HLERequestContext& ctx) {
    LOG_CRITICAL(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::WritePalmaRgbLedPatternEntry(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};
    const auto unknown{rp.Pop<u64>()};

    [[maybe_unused]] const auto buffer = ctx.ReadBuffer();

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, unknown={}",
                connection_handle.npad_id, unknown);

    GetResourceManager()->GetPalma()->WritePalmaRgbLedPatternEntry(connection_handle, unknown);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::WritePalmaWaveEntry(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};
    const auto wave_set{rp.PopEnum<Palma::PalmaWaveSet>()};
    const auto unknown{rp.Pop<u64>()};
    const auto t_mem_size{rp.Pop<u64>()};
    const auto t_mem_handle{ctx.GetCopyHandle(0)};
    const auto size{rp.Pop<u64>()};

    ASSERT_MSG(t_mem_size == 0x3000, "t_mem_size is not 0x3000 bytes");

    auto t_mem = system.ApplicationProcess()->GetHandleTable().GetObject<Kernel::KTransferMemory>(
        t_mem_handle);

    if (t_mem.IsNull()) {
        LOG_ERROR(Service_HID, "t_mem is a nullptr for handle=0x{:08X}", t_mem_handle);
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(ResultUnknown);
        return;
    }

    ASSERT_MSG(t_mem->GetSize() == 0x3000, "t_mem has incorrect size");

    LOG_WARNING(Service_HID,
                "(STUBBED) called, connection_handle={}, wave_set={}, unknown={}, "
                "t_mem_handle=0x{:08X}, t_mem_size={}, size={}",
                connection_handle.npad_id, wave_set, unknown, t_mem_handle, t_mem_size, size);

    GetResourceManager()->GetPalma()->WritePalmaWaveEntry(connection_handle, wave_set,
                                                          t_mem->GetSourceAddress(), t_mem_size);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetPalmaDataBaseIdentificationVersion(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        s32 database_id_version;
        INSERT_PADDING_WORDS_NOINIT(1);
        Palma::PalmaConnectionHandle connection_handle;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}, database_id_version={}",
                parameters.connection_handle.npad_id, parameters.database_id_version);

    GetResourceManager()->GetPalma()->SetPalmaDataBaseIdentificationVersion(
        parameters.connection_handle, parameters.database_id_version);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetPalmaDataBaseIdentificationVersion(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->GetPalmaDataBaseIdentificationVersion(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SuspendPalmaFeature(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetPalmaOperationResult(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    const auto result =
        GetResourceManager()->GetPalma()->GetPalmaOperationResult(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(result);
}

void IHidServer::ReadPalmaPlayLog(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::ResetPalmaPlayLog(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetIsPalmaAllConnectable(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        bool is_palma_all_connectable;
        INSERT_PADDING_BYTES_NOINIT(7);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID,
                "(STUBBED) called, is_palma_all_connectable={},applet_resource_user_id={}",
                parameters.is_palma_all_connectable, parameters.applet_resource_user_id);

    GetResourceManager()->GetPalma()->SetIsPalmaAllConnectable(parameters.is_palma_all_connectable);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetIsPalmaPairedConnectable(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::PairPalma(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto connection_handle{rp.PopRaw<Palma::PalmaConnectionHandle>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, connection_handle={}", connection_handle.npad_id);

    GetResourceManager()->GetPalma()->PairPalma(connection_handle);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetPalmaBoostMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto palma_boost_mode{rp.Pop<bool>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, palma_boost_mode={}", palma_boost_mode);

    GetResourceManager()->GetPalma()->SetPalmaBoostMode(palma_boost_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::CancelWritePalmaWaveEntry(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::EnablePalmaBoostMode(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetPalmaBluetoothAddress(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetDisallowedPalmaConnection(HLERequestContext& ctx) {
    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::SetNpadCommunicationMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto applet_resource_user_id{rp.Pop<u64>()};
    const auto communication_mode{rp.PopEnum<NpadCommunicationMode>()};

    GetResourceManager()->GetNpad()->SetNpadCommunicationMode(communication_mode);

    LOG_WARNING(Service_HID, "(STUBBED) called, applet_resource_user_id={}, communication_mode={}",
                applet_resource_user_id, communication_mode);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::GetNpadCommunicationMode(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};

    LOG_WARNING(Service_HID, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.PushEnum(GetResourceManager()->GetNpad()->GetNpadCommunicationMode());
}

void IHidServer::SetTouchScreenConfiguration(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto touchscreen_mode{rp.PopRaw<Core::HID::TouchScreenConfigurationForNx>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, touchscreen_mode={}, applet_resource_user_id={}",
                touchscreen_mode.mode, applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IHidServer::IsFirmwareUpdateNeededForNotification(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    struct Parameters {
        s32 unknown;
        INSERT_PADDING_WORDS_NOINIT(1);
        u64 applet_resource_user_id;
    };
    static_assert(sizeof(Parameters) == 0x10, "Parameters has incorrect size.");

    const auto parameters{rp.PopRaw<Parameters>()};

    LOG_WARNING(Service_HID, "(STUBBED) called, unknown={}, applet_resource_user_id={}",
                parameters.unknown, parameters.applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 3};
    rb.Push(ResultSuccess);
    rb.Push(false);
}

void IHidServer::SetTouchScreenResolution(HLERequestContext& ctx) {
    IPC::RequestParser rp{ctx};
    const auto width{rp.Pop<u32>()};
    const auto height{rp.Pop<u32>()};
    const auto applet_resource_user_id{rp.Pop<u64>()};

    GetResourceManager()->GetTouchScreen()->SetTouchscreenDimensions(width, height);

    LOG_INFO(Service_HID, "called, width={}, height={}, applet_resource_user_id={}", width, height,
             applet_resource_user_id);

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

std::shared_ptr<ResourceManager> IHidServer::GetResourceManager() {
    resource_manager->Initialize();
    return resource_manager;
}

} // namespace Service::HID
