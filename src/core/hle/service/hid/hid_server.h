// SPDX-FileCopyrightText: Copyright 2023 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Core {
class System;
}

namespace Service::HID {
class ResourceManager;
class HidFirmwareSettings;

class IHidServer final : public ServiceFramework<IHidServer> {
public:
    explicit IHidServer(Core::System& system_, std::shared_ptr<ResourceManager> resource,
                        std::shared_ptr<HidFirmwareSettings> settings);
    ~IHidServer() override;

    std::shared_ptr<ResourceManager> GetResourceManager();

private:
    void CreateAppletResource(HLERequestContext& ctx);
    void ActivateDebugPad(HLERequestContext& ctx);
    void ActivateTouchScreen(HLERequestContext& ctx);
    void ActivateMouse(HLERequestContext& ctx);
    void ActivateKeyboard(HLERequestContext& ctx);
    void SendKeyboardLockKeyEvent(HLERequestContext& ctx);
    void AcquireXpadIdEventHandle(HLERequestContext& ctx);
    void ReleaseXpadIdEventHandle(HLERequestContext& ctx);
    void ActivateXpad(HLERequestContext& ctx);
    void GetXpadIds(HLERequestContext& ctx);
    void ActivateJoyXpad(HLERequestContext& ctx);
    void GetJoyXpadLifoHandle(HLERequestContext& ctx);
    void GetJoyXpadIds(HLERequestContext& ctx);
    void ActivateSixAxisSensor(HLERequestContext& ctx);
    void DeactivateSixAxisSensor(HLERequestContext& ctx);
    void GetSixAxisSensorLifoHandle(HLERequestContext& ctx);
    void ActivateJoySixAxisSensor(HLERequestContext& ctx);
    void DeactivateJoySixAxisSensor(HLERequestContext& ctx);
    void GetJoySixAxisSensorLifoHandle(HLERequestContext& ctx);
    void StartSixAxisSensor(HLERequestContext& ctx);
    void StopSixAxisSensor(HLERequestContext& ctx);
    void IsSixAxisSensorFusionEnabled(HLERequestContext& ctx);
    void EnableSixAxisSensorFusion(HLERequestContext& ctx);
    void SetSixAxisSensorFusionParameters(HLERequestContext& ctx);
    void GetSixAxisSensorFusionParameters(HLERequestContext& ctx);
    void ResetSixAxisSensorFusionParameters(HLERequestContext& ctx);
    void SetGyroscopeZeroDriftMode(HLERequestContext& ctx);
    void GetGyroscopeZeroDriftMode(HLERequestContext& ctx);
    void ResetGyroscopeZeroDriftMode(HLERequestContext& ctx);
    void IsSixAxisSensorAtRest(HLERequestContext& ctx);
    void IsFirmwareUpdateAvailableForSixAxisSensor(HLERequestContext& ctx);
    void EnableSixAxisSensorUnalteredPassthrough(HLERequestContext& ctx);
    void IsSixAxisSensorUnalteredPassthroughEnabled(HLERequestContext& ctx);
    void LoadSixAxisSensorCalibrationParameter(HLERequestContext& ctx);
    void GetSixAxisSensorIcInformation(HLERequestContext& ctx);
    void ResetIsSixAxisSensorDeviceNewlyAssigned(HLERequestContext& ctx);
    void ActivateGesture(HLERequestContext& ctx);
    void SetSupportedNpadStyleSet(HLERequestContext& ctx);
    void GetSupportedNpadStyleSet(HLERequestContext& ctx);
    void SetSupportedNpadIdType(HLERequestContext& ctx);
    void ActivateNpad(HLERequestContext& ctx);
    void DeactivateNpad(HLERequestContext& ctx);
    void AcquireNpadStyleSetUpdateEventHandle(HLERequestContext& ctx);
    void DisconnectNpad(HLERequestContext& ctx);
    void GetPlayerLedPattern(HLERequestContext& ctx);
    void ActivateNpadWithRevision(HLERequestContext& ctx);
    void SetNpadJoyHoldType(HLERequestContext& ctx);
    void GetNpadJoyHoldType(HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingleByDefault(HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingle(HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeDual(HLERequestContext& ctx);
    void MergeSingleJoyAsDualJoy(HLERequestContext& ctx);
    void StartLrAssignmentMode(HLERequestContext& ctx);
    void StopLrAssignmentMode(HLERequestContext& ctx);
    void SetNpadHandheldActivationMode(HLERequestContext& ctx);
    void GetNpadHandheldActivationMode(HLERequestContext& ctx);
    void SwapNpadAssignment(HLERequestContext& ctx);
    void IsUnintendedHomeButtonInputProtectionEnabled(HLERequestContext& ctx);
    void EnableUnintendedHomeButtonInputProtection(HLERequestContext& ctx);
    void SetNpadJoyAssignmentModeSingleWithDestination(HLERequestContext& ctx);
    void SetNpadAnalogStickUseCenterClamp(HLERequestContext& ctx);
    void SetNpadCaptureButtonAssignment(HLERequestContext& ctx);
    void ClearNpadCaptureButtonAssignment(HLERequestContext& ctx);
    void GetVibrationDeviceInfo(HLERequestContext& ctx);
    void SendVibrationValue(HLERequestContext& ctx);
    void GetActualVibrationValue(HLERequestContext& ctx);
    void CreateActiveVibrationDeviceList(HLERequestContext& ctx);
    void PermitVibration(HLERequestContext& ctx);
    void IsVibrationPermitted(HLERequestContext& ctx);
    void SendVibrationValues(HLERequestContext& ctx);
    void SendVibrationGcErmCommand(HLERequestContext& ctx);
    void GetActualVibrationGcErmCommand(HLERequestContext& ctx);
    void BeginPermitVibrationSession(HLERequestContext& ctx);
    void EndPermitVibrationSession(HLERequestContext& ctx);
    void IsVibrationDeviceMounted(HLERequestContext& ctx);
    void SendVibrationValueInBool(HLERequestContext& ctx);
    void ActivateConsoleSixAxisSensor(HLERequestContext& ctx);
    void StartConsoleSixAxisSensor(HLERequestContext& ctx);
    void StopConsoleSixAxisSensor(HLERequestContext& ctx);
    void ActivateSevenSixAxisSensor(HLERequestContext& ctx);
    void StartSevenSixAxisSensor(HLERequestContext& ctx);
    void StopSevenSixAxisSensor(HLERequestContext& ctx);
    void InitializeSevenSixAxisSensor(HLERequestContext& ctx);
    void FinalizeSevenSixAxisSensor(HLERequestContext& ctx);
    void ResetSevenSixAxisSensorTimestamp(HLERequestContext& ctx);
    void IsUsbFullKeyControllerEnabled(HLERequestContext& ctx);
    void GetPalmaConnectionHandle(HLERequestContext& ctx);
    void InitializePalma(HLERequestContext& ctx);
    void AcquirePalmaOperationCompleteEvent(HLERequestContext& ctx);
    void GetPalmaOperationInfo(HLERequestContext& ctx);
    void PlayPalmaActivity(HLERequestContext& ctx);
    void SetPalmaFrModeType(HLERequestContext& ctx);
    void ReadPalmaStep(HLERequestContext& ctx);
    void EnablePalmaStep(HLERequestContext& ctx);
    void ResetPalmaStep(HLERequestContext& ctx);
    void ReadPalmaApplicationSection(HLERequestContext& ctx);
    void WritePalmaApplicationSection(HLERequestContext& ctx);
    void ReadPalmaUniqueCode(HLERequestContext& ctx);
    void SetPalmaUniqueCodeInvalid(HLERequestContext& ctx);
    void WritePalmaActivityEntry(HLERequestContext& ctx);
    void WritePalmaRgbLedPatternEntry(HLERequestContext& ctx);
    void WritePalmaWaveEntry(HLERequestContext& ctx);
    void SetPalmaDataBaseIdentificationVersion(HLERequestContext& ctx);
    void GetPalmaDataBaseIdentificationVersion(HLERequestContext& ctx);
    void SuspendPalmaFeature(HLERequestContext& ctx);
    void GetPalmaOperationResult(HLERequestContext& ctx);
    void ReadPalmaPlayLog(HLERequestContext& ctx);
    void ResetPalmaPlayLog(HLERequestContext& ctx);
    void SetIsPalmaAllConnectable(HLERequestContext& ctx);
    void SetIsPalmaPairedConnectable(HLERequestContext& ctx);
    void PairPalma(HLERequestContext& ctx);
    void SetPalmaBoostMode(HLERequestContext& ctx);
    void CancelWritePalmaWaveEntry(HLERequestContext& ctx);
    void EnablePalmaBoostMode(HLERequestContext& ctx);
    void GetPalmaBluetoothAddress(HLERequestContext& ctx);
    void SetDisallowedPalmaConnection(HLERequestContext& ctx);
    void SetNpadCommunicationMode(HLERequestContext& ctx);
    void GetNpadCommunicationMode(HLERequestContext& ctx);
    void SetTouchScreenConfiguration(HLERequestContext& ctx);
    void IsFirmwareUpdateNeededForNotification(HLERequestContext& ctx);
    void SetTouchScreenResolution(HLERequestContext& ctx);

    std::shared_ptr<ResourceManager> resource_manager;
    std::shared_ptr<HidFirmwareSettings> firmware_settings;
};

} // namespace Service::HID
