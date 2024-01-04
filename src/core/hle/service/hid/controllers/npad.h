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
#include "core/hle/service/hid/controllers/npad/npad_resource.h"
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
class AppletResource;
struct NpadInternalState;
struct NpadSixAxisSensorLifo;
struct NpadSharedMemoryFormat;

class NPad final {
public:
    explicit NPad(Core::HID::HIDCore& hid_core_, KernelHelpers::ServiceContext& service_context_);
    ~NPad();

    Result Activate();
    Result Activate(u64 aruid);

    Result ActivateNpadResource();
    Result ActivateNpadResource(u64 aruid);

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing);

    Result SetSupportedNpadStyleSet(u64 aruid, Core::HID::NpadStyleSet supported_style_set);
    Result GetSupportedNpadStyleSet(u64 aruid,
                                    Core::HID::NpadStyleSet& out_supported_style_set) const;
    Result GetMaskedSupportedNpadStyleSet(u64 aruid,
                                          Core::HID::NpadStyleSet& out_supported_style_set) const;

    Result SetSupportedNpadIdType(u64 aruid,
                                  std::span<const Core::HID::NpadIdType> supported_npad_list);

    Result SetNpadJoyHoldType(u64 aruid, NpadJoyHoldType hold_type);
    Result GetNpadJoyHoldType(u64 aruid, NpadJoyHoldType& out_hold_type) const;

    Result SetNpadHandheldActivationMode(u64 aruid, NpadHandheldActivationMode mode);
    Result GetNpadHandheldActivationMode(u64 aruid, NpadHandheldActivationMode& out_mode) const;

    bool SetNpadMode(u64 aruid, Core::HID::NpadIdType& new_npad_id, Core::HID::NpadIdType npad_id,
                     NpadJoyDeviceType npad_device_type, NpadJoyAssignmentMode assignment_mode);

    bool VibrateControllerAtIndex(u64 aruid, Core::HID::NpadIdType npad_id,
                                  std::size_t device_index,
                                  const Core::HID::VibrationValue& vibration_value);

    void VibrateController(u64 aruid,
                           const Core::HID::VibrationDeviceHandle& vibration_device_handle,
                           const Core::HID::VibrationValue& vibration_value);

    void VibrateControllers(
        u64 aruid, std::span<const Core::HID::VibrationDeviceHandle> vibration_device_handles,
        std::span<const Core::HID::VibrationValue> vibration_values);

    Core::HID::VibrationValue GetLastVibration(
        u64 aruid, const Core::HID::VibrationDeviceHandle& vibration_device_handle) const;

    void InitializeVibrationDevice(const Core::HID::VibrationDeviceHandle& vibration_device_handle);

    void InitializeVibrationDeviceAtIndex(u64 aruid, Core::HID::NpadIdType npad_id,
                                          std::size_t device_index);

    void SetPermitVibrationSession(bool permit_vibration_session);

    bool IsVibrationDeviceMounted(
        u64 aruid, const Core::HID::VibrationDeviceHandle& vibration_device_handle) const;

    Result AcquireNpadStyleSetUpdateEventHandle(u64 aruid, Kernel::KReadableEvent** out_event,
                                                Core::HID::NpadIdType npad_id);

    // Adds a new controller at an index.
    void AddNewControllerAt(u64 aruid, Core::HID::NpadStyleIndex controller,
                            Core::HID::NpadIdType npad_id);
    // Adds a new controller at an index with connection status.
    void UpdateControllerAt(u64 aruid, Core::HID::NpadStyleIndex controller,
                            Core::HID::NpadIdType npad_id, bool connected);

    Result DisconnectNpad(u64 aruid, Core::HID::NpadIdType npad_id);

    Result IsFirmwareUpdateAvailableForSixAxisSensor(
        u64 aruid, const Core::HID::SixAxisSensorHandle& sixaxis_handle,
        bool& is_firmware_available) const;
    Result ResetIsSixAxisSensorDeviceNewlyAssigned(
        u64 aruid, const Core::HID::SixAxisSensorHandle& sixaxis_handle);

    Result GetLedPattern(Core::HID::NpadIdType npad_id, Core::HID::LedPattern& pattern) const;

    Result IsUnintendedHomeButtonInputProtectionEnabled(bool& out_is_enabled, u64 aruid,
                                                        Core::HID::NpadIdType npad_id) const;
    Result EnableUnintendedHomeButtonInputProtection(u64 aruid, Core::HID::NpadIdType npad_id,
                                                     bool is_enabled);

    void SetNpadAnalogStickUseCenterClamp(u64 aruid, bool is_enabled);
    void ClearAllConnectedControllers();
    void DisconnectAllConnectedControllers();
    void ConnectAllDisconnectedControllers();
    void ClearAllControllers();

    Result MergeSingleJoyAsDualJoy(u64 aruid, Core::HID::NpadIdType npad_id_1,
                                   Core::HID::NpadIdType npad_id_2);
    Result StartLrAssignmentMode(u64 aruid);
    Result StopLrAssignmentMode(u64 aruid);
    Result SwapNpadAssignment(u64 aruid, Core::HID::NpadIdType npad_id_1,
                              Core::HID::NpadIdType npad_id_2);

    // Logical OR for all buttons presses on all controllers
    // Specifically for cheat engine and other features.
    Core::HID::NpadButton GetAndResetPressState();

    Result ApplyNpadSystemCommonPolicy(u64 aruid);
    Result ApplyNpadSystemCommonPolicyFull(u64 aruid);
    Result ClearNpadSystemCommonPolicy(u64 aruid);

    void SetRevision(u64 aruid, NpadRevision revision);
    NpadRevision GetRevision(u64 aruid);

    Result RegisterAppletResourceUserId(u64 aruid);
    void UnregisterAppletResourceUserId(u64 aruid);
    void SetNpadExternals(std::shared_ptr<AppletResource> resource,
                          std::recursive_mutex* shared_mutex);

    AppletDetailedUiType GetAppletDetailedUiType(Core::HID::NpadIdType npad_id);

private:
    struct VibrationData {
        bool device_mounted{};
        Core::HID::VibrationValue latest_vibration_value{};
        std::chrono::steady_clock::time_point last_vibration_timepoint{};
    };

    struct NpadControllerData {
        NpadInternalState* shared_memory = nullptr;
        Core::HID::EmulatedController* device = nullptr;

        std::array<VibrationData, 2> vibration{};
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
    void InitNewlyAddedController(u64 aruid, Core::HID::NpadIdType npad_id);
    void RequestPadStateUpdate(u64 aruid, Core::HID::NpadIdType npad_id);
    void WriteEmptyEntry(NpadInternalState* npad);

    NpadControllerData& GetControllerFromHandle(
        u64 aruid, const Core::HID::VibrationDeviceHandle& device_handle);
    const NpadControllerData& GetControllerFromHandle(
        u64 aruid, const Core::HID::VibrationDeviceHandle& device_handle) const;
    NpadControllerData& GetControllerFromHandle(
        u64 aruid, const Core::HID::SixAxisSensorHandle& device_handle);
    const NpadControllerData& GetControllerFromHandle(
        u64 aruid, const Core::HID::SixAxisSensorHandle& device_handle) const;
    NpadControllerData& GetControllerFromNpadIdType(u64 aruid, Core::HID::NpadIdType npad_id);
    const NpadControllerData& GetControllerFromNpadIdType(u64 aruid,
                                                          Core::HID::NpadIdType npad_id) const;

    Core::HID::SixAxisSensorProperties& GetSixaxisProperties(
        u64 aruid, const Core::HID::SixAxisSensorHandle& device_handle);
    const Core::HID::SixAxisSensorProperties& GetSixaxisProperties(
        u64 aruid, const Core::HID::SixAxisSensorHandle& device_handle) const;

    Core::HID::HIDCore& hid_core;
    KernelHelpers::ServiceContext& service_context;

    s32 ref_counter{};
    mutable std::mutex mutex;
    NPadResource npad_resource;
    AppletResourceHolder applet_resource_holder{};
    Kernel::KEvent* input_event{nullptr};
    std::mutex* input_mutex{nullptr};

    std::atomic<u64> press_state{};
    bool permit_vibration_session_enabled;
    std::array<std::array<NpadControllerData, MaxSupportedNpadIdTypes>, AruidIndexMax>
        controller_data{};
};
} // namespace Service::HID
