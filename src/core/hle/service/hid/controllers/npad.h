// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>
#include <atomic>
#include <mutex>
#include <span>

#include "common/common_types.h"
#include "core/hid/hid_types.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/types/npad_types.h"

namespace Core::HID {
class EmulatedController;
enum class ControllerTriggerType;
} // namespace Core::HID

namespace Kernel {
class KEvent;
class KReadableEvent;
} // namespace Kernel

namespace Service::KernelHelpers {
class ServiceContext;
} // namespace Service::KernelHelpers

union Result;

namespace Service::HID {
struct NpadInternalState;
struct NpadSixAxisSensorLifo;
struct NpadSharedMemoryFormat;

class NPad final : public ControllerBase {
public:
    explicit NPad(Core::HID::HIDCore& hid_core_, NpadSharedMemoryFormat& npad_shared_memory_format,
                  KernelHelpers::ServiceContext& service_context_);
    ~NPad() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

    void SetSupportedStyleSet(Core::HID::NpadStyleTag style_set);
    Core::HID::NpadStyleTag GetSupportedStyleSet() const;

    Result SetSupportedNpadIdTypes(std::span<const u8> data);
    void GetSupportedNpadIdTypes(u32* data, std::size_t max_length);
    std::size_t GetSupportedNpadIdTypesSize() const;

    void SetHoldType(NpadJoyHoldType joy_hold_type);
    NpadJoyHoldType GetHoldType() const;

    void SetNpadHandheldActivationMode(NpadHandheldActivationMode activation_mode);
    NpadHandheldActivationMode GetNpadHandheldActivationMode() const;

    void SetNpadCommunicationMode(NpadCommunicationMode communication_mode_);
    NpadCommunicationMode GetNpadCommunicationMode() const;

    bool SetNpadMode(Core::HID::NpadIdType& new_npad_id, Core::HID::NpadIdType npad_id,
                     NpadJoyDeviceType npad_device_type, NpadJoyAssignmentMode assignment_mode);

    bool VibrateControllerAtIndex(Core::HID::NpadIdType npad_id, std::size_t device_index,
                                  const Core::HID::VibrationValue& vibration_value);

    void VibrateController(const Core::HID::VibrationDeviceHandle& vibration_device_handle,
                           const Core::HID::VibrationValue& vibration_value);

    void VibrateControllers(
        std::span<const Core::HID::VibrationDeviceHandle> vibration_device_handles,
        std::span<const Core::HID::VibrationValue> vibration_values);

    Core::HID::VibrationValue GetLastVibration(
        const Core::HID::VibrationDeviceHandle& vibration_device_handle) const;

    void InitializeVibrationDevice(const Core::HID::VibrationDeviceHandle& vibration_device_handle);

    void InitializeVibrationDeviceAtIndex(Core::HID::NpadIdType npad_id, std::size_t device_index);

    void SetPermitVibrationSession(bool permit_vibration_session);

    bool IsVibrationDeviceMounted(
        const Core::HID::VibrationDeviceHandle& vibration_device_handle) const;

    Kernel::KReadableEvent& GetStyleSetChangedEvent(Core::HID::NpadIdType npad_id);
    void SignalStyleSetChangedEvent(Core::HID::NpadIdType npad_id) const;

    // Adds a new controller at an index.
    void AddNewControllerAt(Core::HID::NpadStyleIndex controller, Core::HID::NpadIdType npad_id);
    // Adds a new controller at an index with connection status.
    void UpdateControllerAt(Core::HID::NpadStyleIndex controller, Core::HID::NpadIdType npad_id,
                            bool connected);

    Result DisconnectNpad(Core::HID::NpadIdType npad_id);

    Result IsFirmwareUpdateAvailableForSixAxisSensor(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle, bool& is_firmware_available) const;
    Result ResetIsSixAxisSensorDeviceNewlyAssigned(
        const Core::HID::SixAxisSensorHandle& sixaxis_handle);

    NpadSixAxisSensorLifo& GetSixAxisFullkeyLifo(Core::HID::NpadIdType npad_id);
    NpadSixAxisSensorLifo& GetSixAxisHandheldLifo(Core::HID::NpadIdType npad_id);
    NpadSixAxisSensorLifo& GetSixAxisDualLeftLifo(Core::HID::NpadIdType npad_id);
    NpadSixAxisSensorLifo& GetSixAxisDualRightLifo(Core::HID::NpadIdType npad_id);
    NpadSixAxisSensorLifo& GetSixAxisLeftLifo(Core::HID::NpadIdType npad_id);
    NpadSixAxisSensorLifo& GetSixAxisRightLifo(Core::HID::NpadIdType npad_id);

    Result GetLedPattern(Core::HID::NpadIdType npad_id, Core::HID::LedPattern& pattern) const;
    Result IsUnintendedHomeButtonInputProtectionEnabled(Core::HID::NpadIdType npad_id,
                                                        bool& is_enabled) const;
    Result SetUnintendedHomeButtonInputProtectionEnabled(bool is_protection_enabled,
                                                         Core::HID::NpadIdType npad_id);
    void SetAnalogStickUseCenterClamp(bool use_center_clamp);
    void ClearAllConnectedControllers();
    void DisconnectAllConnectedControllers();
    void ConnectAllDisconnectedControllers();
    void ClearAllControllers();

    Result MergeSingleJoyAsDualJoy(Core::HID::NpadIdType npad_id_1,
                                   Core::HID::NpadIdType npad_id_2);
    void StartLRAssignmentMode();
    void StopLRAssignmentMode();
    Result SwapNpadAssignment(Core::HID::NpadIdType npad_id_1, Core::HID::NpadIdType npad_id_2);

    // Logical OR for all buttons presses on all controllers
    // Specifically for cheat engine and other features.
    Core::HID::NpadButton GetAndResetPressState();

    void ApplyNpadSystemCommonPolicy();

    AppletDetailedUiType GetAppletDetailedUiType(Core::HID::NpadIdType npad_id);

private:
    struct VibrationData {
        bool device_mounted{};
        Core::HID::VibrationValue latest_vibration_value{};
        std::chrono::steady_clock::time_point last_vibration_timepoint{};
    };

    struct NpadControllerData {
        Kernel::KEvent* styleset_changed_event{};
        NpadInternalState* shared_memory = nullptr;
        Core::HID::EmulatedController* device = nullptr;

        std::array<VibrationData, 2> vibration{};
        bool unintended_home_button_input_protection{};
        bool is_connected{};

        // Dual joycons can have only one side connected
        bool is_dual_left_connected{true};
        bool is_dual_right_connected{true};

        // Current pad state
        NPadGenericState npad_pad_state{};
        NPadGenericState npad_libnx_state{};
        NpadGcTriggerState npad_trigger_state{};
        int callback_key{};
    };

    void ControllerUpdate(Core::HID::ControllerTriggerType type, std::size_t controller_idx);
    void InitNewlyAddedController(Core::HID::NpadIdType npad_id);
    bool IsControllerSupported(Core::HID::NpadStyleIndex controller) const;
    void RequestPadStateUpdate(Core::HID::NpadIdType npad_id);
    void WriteEmptyEntry(NpadInternalState* npad);

    NpadControllerData& GetControllerFromHandle(
        const Core::HID::VibrationDeviceHandle& device_handle);
    const NpadControllerData& GetControllerFromHandle(
        const Core::HID::VibrationDeviceHandle& device_handle) const;
    NpadControllerData& GetControllerFromHandle(
        const Core::HID::SixAxisSensorHandle& device_handle);
    const NpadControllerData& GetControllerFromHandle(
        const Core::HID::SixAxisSensorHandle& device_handle) const;
    NpadControllerData& GetControllerFromNpadIdType(Core::HID::NpadIdType npad_id);
    const NpadControllerData& GetControllerFromNpadIdType(Core::HID::NpadIdType npad_id) const;

    Core::HID::SixAxisSensorProperties& GetSixaxisProperties(
        const Core::HID::SixAxisSensorHandle& device_handle);
    const Core::HID::SixAxisSensorProperties& GetSixaxisProperties(
        const Core::HID::SixAxisSensorHandle& device_handle) const;

    std::atomic<u64> press_state{};

    std::array<NpadControllerData, NpadCount> controller_data{};
    KernelHelpers::ServiceContext& service_context;
    std::mutex mutex;
    std::vector<Core::HID::NpadIdType> supported_npad_id_types{};
    NpadJoyHoldType hold_type{NpadJoyHoldType::Vertical};
    NpadHandheldActivationMode handheld_activation_mode{NpadHandheldActivationMode::Dual};
    NpadCommunicationMode communication_mode{NpadCommunicationMode::Default};
    bool permit_vibration_session_enabled{false};
    bool analog_stick_use_center_clamp{false};
    bool is_in_lr_assignment_mode{false};
    bool is_controller_initialized{false};
};
} // namespace Service::HID
