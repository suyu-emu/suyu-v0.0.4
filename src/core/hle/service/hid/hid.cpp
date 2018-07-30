// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/core_timing_util.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/input.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/irs.h"
#include "core/hle/service/hid/xcd.h"
#include "core/hle/service/service.h"

namespace Service::HID {

// Updating period for each HID device.
// TODO(shinyquagsire23): These need better values.
constexpr u64 pad_update_ticks = CoreTiming::BASE_CLOCK_RATE / 100;
constexpr u64 accelerometer_update_ticks = CoreTiming::BASE_CLOCK_RATE / 100;
constexpr u64 gyroscope_update_ticks = CoreTiming::BASE_CLOCK_RATE / 100;

class IAppletResource final : public ServiceFramework<IAppletResource> {
public:
    IAppletResource() : ServiceFramework("IAppletResource") {
        static const FunctionInfo functions[] = {
            {0, &IAppletResource::GetSharedMemoryHandle, "GetSharedMemoryHandle"},
        };
        RegisterHandlers(functions);

        shared_mem = Kernel::SharedMemory::Create(
            nullptr, 0x40000, Kernel::MemoryPermission::ReadWrite, Kernel::MemoryPermission::Read,
            0, Kernel::MemoryRegion::BASE, "HID:SharedMemory");

        // Register update callbacks
        pad_update_event = CoreTiming::RegisterEvent(
            "HID::UpdatePadCallback",
            [this](u64 userdata, int cycles_late) { UpdatePadCallback(userdata, cycles_late); });

        // TODO(shinyquagsire23): Other update callbacks? (accel, gyro?)

        CoreTiming::ScheduleEvent(pad_update_ticks, pad_update_event);
    }

    ~IAppletResource() {
        CoreTiming::UnscheduleEvent(pad_update_event, 0);
    }

private:
    void GetSharedMemoryHandle(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(shared_mem);
        LOG_DEBUG(Service_HID, "called");
    }

    void LoadInputDevices() {
        std::transform(Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                       Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_END,
                       buttons.begin(), Input::CreateDevice<Input::ButtonDevice>);
        std::transform(Settings::values.analogs.begin() + Settings::NativeAnalog::STICK_HID_BEGIN,
                       Settings::values.analogs.begin() + Settings::NativeAnalog::STICK_HID_END,
                       sticks.begin(), Input::CreateDevice<Input::AnalogDevice>);
        touch_device = Input::CreateDevice<Input::TouchDevice>(Settings::values.touch_device);
        // TODO(shinyquagsire23): gyro, mouse, keyboard
    }

    void UpdatePadCallback(u64 userdata, int cycles_late) {
        SharedMemory mem{};
        std::memcpy(&mem, shared_mem->GetPointer(), sizeof(SharedMemory));

        if (is_device_reload_pending.exchange(false))
            LoadInputDevices();

        // Set up controllers as neon red+blue Joy-Con attached to console
        ControllerHeader& controller_header = mem.controllers[Controller_Handheld].header;
        controller_header.type = ControllerType_Handheld;
        controller_header.single_colors_descriptor = ColorDesc_ColorsNonexistent;
        controller_header.right_color_body = JOYCON_BODY_NEON_RED;
        controller_header.right_color_buttons = JOYCON_BUTTONS_NEON_RED;
        controller_header.left_color_body = JOYCON_BODY_NEON_BLUE;
        controller_header.left_color_buttons = JOYCON_BUTTONS_NEON_BLUE;

        for (size_t controller = 0; controller < mem.controllers.size(); controller++) {
            for (auto& layout : mem.controllers[controller].layouts) {
                layout.header.num_entries = HID_NUM_ENTRIES;
                layout.header.max_entry_index = HID_NUM_ENTRIES - 1;

                // HID shared memory stores the state of the past 17 samples in a circlular buffer,
                // each with a timestamp in number of samples since boot.
                const ControllerInputEntry& last_entry = layout.entries[layout.header.latest_entry];

                layout.header.timestamp_ticks = CoreTiming::GetTicks();
                layout.header.latest_entry = (layout.header.latest_entry + 1) % HID_NUM_ENTRIES;

                ControllerInputEntry& entry = layout.entries[layout.header.latest_entry];
                entry.timestamp = last_entry.timestamp + 1;
                // TODO(shinyquagsire23): Is this always identical to timestamp?
                entry.timestamp_2 = entry.timestamp;

                // TODO(shinyquagsire23): More than just handheld input
                if (controller != Controller_Handheld)
                    continue;

                entry.connection_state = ConnectionState_Connected | ConnectionState_Wired;

                // TODO(shinyquagsire23): Set up some LUTs for each layout mapping in the future?
                // For now everything is just the default handheld layout, but split Joy-Con will
                // rotate the face buttons and directions for certain layouts.
                ControllerPadState& state = entry.buttons;
                using namespace Settings::NativeButton;
                state.a.Assign(buttons[A - BUTTON_HID_BEGIN]->GetStatus());
                state.b.Assign(buttons[B - BUTTON_HID_BEGIN]->GetStatus());
                state.x.Assign(buttons[X - BUTTON_HID_BEGIN]->GetStatus());
                state.y.Assign(buttons[Y - BUTTON_HID_BEGIN]->GetStatus());
                state.lstick.Assign(buttons[LStick - BUTTON_HID_BEGIN]->GetStatus());
                state.rstick.Assign(buttons[RStick - BUTTON_HID_BEGIN]->GetStatus());
                state.l.Assign(buttons[L - BUTTON_HID_BEGIN]->GetStatus());
                state.r.Assign(buttons[R - BUTTON_HID_BEGIN]->GetStatus());
                state.zl.Assign(buttons[ZL - BUTTON_HID_BEGIN]->GetStatus());
                state.zr.Assign(buttons[ZR - BUTTON_HID_BEGIN]->GetStatus());
                state.plus.Assign(buttons[Plus - BUTTON_HID_BEGIN]->GetStatus());
                state.minus.Assign(buttons[Minus - BUTTON_HID_BEGIN]->GetStatus());

                state.dleft.Assign(buttons[DLeft - BUTTON_HID_BEGIN]->GetStatus());
                state.dup.Assign(buttons[DUp - BUTTON_HID_BEGIN]->GetStatus());
                state.dright.Assign(buttons[DRight - BUTTON_HID_BEGIN]->GetStatus());
                state.ddown.Assign(buttons[DDown - BUTTON_HID_BEGIN]->GetStatus());

                state.lstick_left.Assign(buttons[LStick_Left - BUTTON_HID_BEGIN]->GetStatus());
                state.lstick_up.Assign(buttons[LStick_Up - BUTTON_HID_BEGIN]->GetStatus());
                state.lstick_right.Assign(buttons[LStick_Right - BUTTON_HID_BEGIN]->GetStatus());
                state.lstick_down.Assign(buttons[LStick_Down - BUTTON_HID_BEGIN]->GetStatus());

                state.rstick_left.Assign(buttons[RStick_Left - BUTTON_HID_BEGIN]->GetStatus());
                state.rstick_up.Assign(buttons[RStick_Up - BUTTON_HID_BEGIN]->GetStatus());
                state.rstick_right.Assign(buttons[RStick_Right - BUTTON_HID_BEGIN]->GetStatus());
                state.rstick_down.Assign(buttons[RStick_Down - BUTTON_HID_BEGIN]->GetStatus());

                state.sl.Assign(buttons[SL - BUTTON_HID_BEGIN]->GetStatus());
                state.sr.Assign(buttons[SR - BUTTON_HID_BEGIN]->GetStatus());

                const auto [stick_l_x_f, stick_l_y_f] = sticks[Joystick_Left]->GetStatus();
                const auto [stick_r_x_f, stick_r_y_f] = sticks[Joystick_Right]->GetStatus();
                entry.joystick_left_x = static_cast<s32>(stick_l_x_f * HID_JOYSTICK_MAX);
                entry.joystick_left_y = static_cast<s32>(stick_l_y_f * HID_JOYSTICK_MAX);
                entry.joystick_right_x = static_cast<s32>(stick_r_x_f * HID_JOYSTICK_MAX);
                entry.joystick_right_y = static_cast<s32>(stick_r_y_f * HID_JOYSTICK_MAX);
            }
        }

        TouchScreen& touchscreen = mem.touchscreen;
        const u64 last_entry = touchscreen.header.latest_entry;
        const u64 curr_entry = (last_entry + 1) % touchscreen.entries.size();
        const u64 timestamp = CoreTiming::GetTicks();
        const u64 sample_counter = touchscreen.entries[last_entry].header.timestamp + 1;
        touchscreen.header.timestamp_ticks = timestamp;
        touchscreen.header.num_entries = touchscreen.entries.size();
        touchscreen.header.latest_entry = curr_entry;
        touchscreen.header.max_entry_index = touchscreen.entries.size();
        touchscreen.header.timestamp = timestamp;
        touchscreen.entries[curr_entry].header.timestamp = sample_counter;

        TouchScreenEntryTouch touch_entry{};
        auto [x, y, pressed] = touch_device->GetStatus();
        touch_entry.timestamp = timestamp;
        touch_entry.x = static_cast<u16>(x * Layout::ScreenUndocked::Width);
        touch_entry.y = static_cast<u16>(y * Layout::ScreenUndocked::Height);
        touch_entry.touch_index = 0;

        // TODO(DarkLordZach): Maybe try to derive these from EmuWindow?
        touch_entry.diameter_x = 15;
        touch_entry.diameter_y = 15;
        touch_entry.angle = 0;

        // TODO(DarkLordZach): Implement multi-touch support
        if (pressed) {
            touchscreen.entries[curr_entry].header.num_touches = 1;
            touchscreen.entries[curr_entry].touches[0] = touch_entry;
        } else {
            touchscreen.entries[curr_entry].header.num_touches = 0;
        }

        // TODO(shinyquagsire23): Properly implement mouse
        Mouse& mouse = mem.mouse;
        const u64 last_mouse_entry = mouse.header.latest_entry;
        const u64 curr_mouse_entry = (mouse.header.latest_entry + 1) % mouse.entries.size();
        const u64 mouse_sample_counter = mouse.entries[last_mouse_entry].timestamp + 1;
        mouse.header.timestamp_ticks = timestamp;
        mouse.header.num_entries = mouse.entries.size();
        mouse.header.max_entry_index = mouse.entries.size();
        mouse.header.latest_entry = curr_mouse_entry;

        mouse.entries[curr_mouse_entry].timestamp = mouse_sample_counter;
        mouse.entries[curr_mouse_entry].timestamp_2 = mouse_sample_counter;

        // TODO(shinyquagsire23): Properly implement keyboard
        Keyboard& keyboard = mem.keyboard;
        const u64 last_keyboard_entry = keyboard.header.latest_entry;
        const u64 curr_keyboard_entry =
            (keyboard.header.latest_entry + 1) % keyboard.entries.size();
        const u64 keyboard_sample_counter = keyboard.entries[last_keyboard_entry].timestamp + 1;
        keyboard.header.timestamp_ticks = timestamp;
        keyboard.header.num_entries = keyboard.entries.size();
        keyboard.header.latest_entry = last_keyboard_entry;
        keyboard.header.max_entry_index = keyboard.entries.size();

        keyboard.entries[curr_keyboard_entry].timestamp = keyboard_sample_counter;
        keyboard.entries[curr_keyboard_entry].timestamp_2 = keyboard_sample_counter;

        // TODO(shinyquagsire23): Figure out what any of these are
        for (auto& input : mem.unk_input_1) {
            const u64 last_input_entry = input.header.latest_entry;
            const u64 curr_input_entry = (input.header.latest_entry + 1) % input.entries.size();
            const u64 input_sample_counter = input.entries[last_input_entry].timestamp + 1;

            input.header.timestamp_ticks = timestamp;
            input.header.num_entries = input.entries.size();
            input.header.latest_entry = last_input_entry;
            input.header.max_entry_index = input.entries.size();

            input.entries[curr_input_entry].timestamp = input_sample_counter;
            input.entries[curr_input_entry].timestamp_2 = input_sample_counter;
        }

        for (auto& input : mem.unk_input_2) {
            input.header.timestamp_ticks = timestamp;
            input.header.num_entries = 17;
            input.header.latest_entry = 0;
            input.header.max_entry_index = 0;
        }

        UnkInput3& input = mem.unk_input_3;
        const u64 last_input_entry = input.header.latest_entry;
        const u64 curr_input_entry = (input.header.latest_entry + 1) % input.entries.size();
        const u64 input_sample_counter = input.entries[last_input_entry].timestamp + 1;

        input.header.timestamp_ticks = timestamp;
        input.header.num_entries = input.entries.size();
        input.header.latest_entry = last_input_entry;
        input.header.max_entry_index = input.entries.size();

        input.entries[curr_input_entry].timestamp = input_sample_counter;
        input.entries[curr_input_entry].timestamp_2 = input_sample_counter;

        // TODO(shinyquagsire23): Signal events

        std::memcpy(shared_mem->GetPointer(), &mem, sizeof(SharedMemory));

        // Reschedule recurrent event
        CoreTiming::ScheduleEvent(pad_update_ticks - cycles_late, pad_update_event);
    }

    // Handle to shared memory region designated to HID service
    Kernel::SharedPtr<Kernel::SharedMemory> shared_mem;

    // CoreTiming update events
    CoreTiming::EventType* pad_update_event;

    // Stored input state info
    std::atomic<bool> is_device_reload_pending{true};
    std::array<std::unique_ptr<Input::ButtonDevice>, Settings::NativeButton::NUM_BUTTONS_HID>
        buttons;
    std::array<std::unique_ptr<Input::AnalogDevice>, Settings::NativeAnalog::NUM_STICKS_HID> sticks;
    std::unique_ptr<Input::TouchDevice> touch_device;
};

class IActiveVibrationDeviceList final : public ServiceFramework<IActiveVibrationDeviceList> {
public:
    IActiveVibrationDeviceList() : ServiceFramework("IActiveVibrationDeviceList") {
        static const FunctionInfo functions[] = {
            {0, &IActiveVibrationDeviceList::ActivateVibrationDevice, "ActivateVibrationDevice"},
        };
        RegisterHandlers(functions);
    }

private:
    void ActivateVibrationDevice(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }
};

class Hid final : public ServiceFramework<Hid> {
public:
    Hid() : ServiceFramework("hid") {
        static const FunctionInfo functions[] = {
            {0, &Hid::CreateAppletResource, "CreateAppletResource"},
            {1, &Hid::ActivateDebugPad, "ActivateDebugPad"},
            {11, &Hid::ActivateTouchScreen, "ActivateTouchScreen"},
            {21, &Hid::ActivateMouse, "ActivateMouse"},
            {31, &Hid::ActivateKeyboard, "ActivateKeyboard"},
            {40, nullptr, "AcquireXpadIdEventHandle"},
            {41, nullptr, "ReleaseXpadIdEventHandle"},
            {51, nullptr, "ActivateXpad"},
            {55, nullptr, "GetXpadIds"},
            {56, nullptr, "ActivateJoyXpad"},
            {58, nullptr, "GetJoyXpadLifoHandle"},
            {59, nullptr, "GetJoyXpadIds"},
            {60, nullptr, "ActivateSixAxisSensor"},
            {61, nullptr, "DeactivateSixAxisSensor"},
            {62, nullptr, "GetSixAxisSensorLifoHandle"},
            {63, nullptr, "ActivateJoySixAxisSensor"},
            {64, nullptr, "DeactivateJoySixAxisSensor"},
            {65, nullptr, "GetJoySixAxisSensorLifoHandle"},
            {66, &Hid::StartSixAxisSensor, "StartSixAxisSensor"},
            {67, nullptr, "StopSixAxisSensor"},
            {68, nullptr, "IsSixAxisSensorFusionEnabled"},
            {69, nullptr, "EnableSixAxisSensorFusion"},
            {70, nullptr, "SetSixAxisSensorFusionParameters"},
            {71, nullptr, "GetSixAxisSensorFusionParameters"},
            {72, nullptr, "ResetSixAxisSensorFusionParameters"},
            {73, nullptr, "SetAccelerometerParameters"},
            {74, nullptr, "GetAccelerometerParameters"},
            {75, nullptr, "ResetAccelerometerParameters"},
            {76, nullptr, "SetAccelerometerPlayMode"},
            {77, nullptr, "GetAccelerometerPlayMode"},
            {78, nullptr, "ResetAccelerometerPlayMode"},
            {79, &Hid::SetGyroscopeZeroDriftMode, "SetGyroscopeZeroDriftMode"},
            {80, nullptr, "GetGyroscopeZeroDriftMode"},
            {81, nullptr, "ResetGyroscopeZeroDriftMode"},
            {82, &Hid::IsSixAxisSensorAtRest, "IsSixAxisSensorAtRest"},
            {91, nullptr, "ActivateGesture"},
            {100, &Hid::SetSupportedNpadStyleSet, "SetSupportedNpadStyleSet"},
            {101, &Hid::GetSupportedNpadStyleSet, "GetSupportedNpadStyleSet"},
            {102, &Hid::SetSupportedNpadIdType, "SetSupportedNpadIdType"},
            {103, &Hid::ActivateNpad, "ActivateNpad"},
            {104, nullptr, "DeactivateNpad"},
            {106, &Hid::AcquireNpadStyleSetUpdateEventHandle,
             "AcquireNpadStyleSetUpdateEventHandle"},
            {107, nullptr, "DisconnectNpad"},
            {108, &Hid::GetPlayerLedPattern, "GetPlayerLedPattern"},
            {120, &Hid::SetNpadJoyHoldType, "SetNpadJoyHoldType"},
            {121, &Hid::GetNpadJoyHoldType, "GetNpadJoyHoldType"},
            {122, &Hid::SetNpadJoyAssignmentModeSingleByDefault,
             "SetNpadJoyAssignmentModeSingleByDefault"},
            {123, nullptr, "SetNpadJoyAssignmentModeSingleByDefault"},
            {124, &Hid::SetNpadJoyAssignmentModeDual, "SetNpadJoyAssignmentModeDual"},
            {125, &Hid::MergeSingleJoyAsDualJoy, "MergeSingleJoyAsDualJoy"},
            {126, nullptr, "StartLrAssignmentMode"},
            {127, nullptr, "StopLrAssignmentMode"},
            {128, &Hid::SetNpadHandheldActivationMode, "SetNpadHandheldActivationMode"},
            {129, nullptr, "GetNpadHandheldActivationMode"},
            {130, nullptr, "SwapNpadAssignment"},
            {131, nullptr, "IsUnintendedHomeButtonInputProtectionEnabled"},
            {132, nullptr, "EnableUnintendedHomeButtonInputProtection"},
            {133, nullptr, "SetNpadJoyAssignmentModeSingleWithDestination"},
            {200, &Hid::GetVibrationDeviceInfo, "GetVibrationDeviceInfo"},
            {201, &Hid::SendVibrationValue, "SendVibrationValue"},
            {202, &Hid::GetActualVibrationValue, "GetActualVibrationValue"},
            {203, &Hid::CreateActiveVibrationDeviceList, "CreateActiveVibrationDeviceList"},
            {204, nullptr, "PermitVibration"},
            {205, nullptr, "IsVibrationPermitted"},
            {206, &Hid::SendVibrationValues, "SendVibrationValues"},
            {207, nullptr, "SendVibrationGcErmCommand"},
            {208, nullptr, "GetActualVibrationGcErmCommand"},
            {209, nullptr, "BeginPermitVibrationSession"},
            {210, nullptr, "EndPermitVibrationSession"},
            {300, nullptr, "ActivateConsoleSixAxisSensor"},
            {301, nullptr, "StartConsoleSixAxisSensor"},
            {302, nullptr, "StopConsoleSixAxisSensor"},
            {303, nullptr, "ActivateSevenSixAxisSensor"},
            {304, nullptr, "StartSevenSixAxisSensor"},
            {305, nullptr, "StopSevenSixAxisSensor"},
            {306, nullptr, "InitializeSevenSixAxisSensor"},
            {307, nullptr, "FinalizeSevenSixAxisSensor"},
            {308, nullptr, "SetSevenSixAxisSensorFusionStrength"},
            {309, nullptr, "GetSevenSixAxisSensorFusionStrength"},
            {400, nullptr, "IsUsbFullKeyControllerEnabled"},
            {401, nullptr, "EnableUsbFullKeyController"},
            {402, nullptr, "IsUsbFullKeyControllerConnected"},
            {403, nullptr, "HasBattery"},
            {404, nullptr, "HasLeftRightBattery"},
            {405, nullptr, "GetNpadInterfaceType"},
            {406, nullptr, "GetNpadLeftRightInterfaceType"},
            {500, nullptr, "GetPalmaConnectionHandle"},
            {501, nullptr, "InitializePalma"},
            {502, nullptr, "AcquirePalmaOperationCompleteEvent"},
            {503, nullptr, "GetPalmaOperationInfo"},
            {504, nullptr, "PlayPalmaActivity"},
            {505, nullptr, "SetPalmaFrModeType"},
            {506, nullptr, "ReadPalmaStep"},
            {507, nullptr, "EnablePalmaStep"},
            {508, nullptr, "SuspendPalmaStep"},
            {509, nullptr, "ResetPalmaStep"},
            {510, nullptr, "ReadPalmaApplicationSection"},
            {511, nullptr, "WritePalmaApplicationSection"},
            {512, nullptr, "ReadPalmaUniqueCode"},
            {513, nullptr, "SetPalmaUniqueCodeInvalid"},
            {1000, nullptr, "SetNpadCommunicationMode"},
            {1001, nullptr, "GetNpadCommunicationMode"},
        };
        RegisterHandlers(functions);

        event = Kernel::Event::Create(Kernel::ResetType::OneShot, "hid:EventHandle");
    }
    ~Hid() = default;

private:
    std::shared_ptr<IAppletResource> applet_resource;
    u32 joy_hold_type{0};
    Kernel::SharedPtr<Kernel::Event> event;

    void CreateAppletResource(Kernel::HLERequestContext& ctx) {
        if (applet_resource == nullptr) {
            applet_resource = std::make_shared<IAppletResource>();
        }

        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IAppletResource>(applet_resource);
        LOG_DEBUG(Service_HID, "called");
    }

    void ActivateDebugPad(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void ActivateTouchScreen(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void ActivateMouse(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void ActivateKeyboard(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void StartSixAxisSensor(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetGyroscopeZeroDriftMode(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void IsSixAxisSensorAtRest(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        // TODO (Hexagon12): Properly implement reading gyroscope values from controllers.
        rb.Push(true);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void GetSupportedNpadStyleSet(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u32>(0);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetSupportedNpadIdType(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void ActivateNpad(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void AcquireNpadStyleSetUpdateEventHandle(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(event);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void GetPlayerLedPattern(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetNpadJoyHoldType(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void GetNpadJoyHoldType(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 3};
        rb.Push(RESULT_SUCCESS);
        rb.Push(joy_hold_type);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetNpadJoyAssignmentModeSingleByDefault(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SendVibrationValue(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void GetActualVibrationValue(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetNpadJoyAssignmentModeDual(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void MergeSingleJoyAsDualJoy(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void SetNpadHandheldActivationMode(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void GetVibrationDeviceInfo(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 4};
        rb.Push(RESULT_SUCCESS);
        rb.Push<u64>(0);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }

    void CreateActiveVibrationDeviceList(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2, 0, 1};
        rb.Push(RESULT_SUCCESS);
        rb.PushIpcInterface<IActiveVibrationDeviceList>();
        LOG_DEBUG(Service_HID, "called");
    }

    void SendVibrationValues(Kernel::HLERequestContext& ctx) {
        IPC::ResponseBuilder rb{ctx, 2};
        rb.Push(RESULT_SUCCESS);
        LOG_WARNING(Service_HID, "(STUBBED) called");
    }
};

class HidDbg final : public ServiceFramework<HidDbg> {
public:
    explicit HidDbg() : ServiceFramework{"hid:dbg"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "DeactivateDebugPad"},
            {1, nullptr, "SetDebugPadAutoPilotState"},
            {2, nullptr, "UnsetDebugPadAutoPilotState"},
            {10, nullptr, "DeactivateTouchScreen"},
            {11, nullptr, "SetTouchScreenAutoPilotState"},
            {12, nullptr, "UnsetTouchScreenAutoPilotState"},
            {20, nullptr, "DeactivateMouse"},
            {21, nullptr, "SetMouseAutoPilotState"},
            {22, nullptr, "UnsetMouseAutoPilotState"},
            {30, nullptr, "DeactivateKeyboard"},
            {31, nullptr, "SetKeyboardAutoPilotState"},
            {32, nullptr, "UnsetKeyboardAutoPilotState"},
            {50, nullptr, "DeactivateXpad"},
            {51, nullptr, "SetXpadAutoPilotState"},
            {52, nullptr, "UnsetXpadAutoPilotState"},
            {60, nullptr, "DeactivateJoyXpad"},
            {91, nullptr, "DeactivateGesture"},
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
            {221, nullptr, "UpdateControllerColor"},
            {222, nullptr, "ConnectUsbPadsAsync"},
            {223, nullptr, "DisconnectUsbPadsAsync"},
            {224, nullptr, "UpdateDesignInfo"},
            {225, nullptr, "GetUniquePadDriverState"},
            {226, nullptr, "GetSixAxisSensorDriverStates"},
            {301, nullptr, "GetAbstractedPadHandles"},
            {302, nullptr, "GetAbstractedPadState"},
            {303, nullptr, "GetAbstractedPadsState"},
            {321, nullptr, "SetAutoPilotVirtualPadState"},
            {322, nullptr, "UnsetAutoPilotVirtualPadState"},
            {323, nullptr, "UnsetAllAutoPilotVirtualPadState"},
            {350, nullptr, "AddRegisteredDevice"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class HidSys final : public ServiceFramework<HidSys> {
public:
    explicit HidSys() : ServiceFramework{"hid:sys"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {31, nullptr, "SendKeyboardLockKeyEvent"},
            {101, nullptr, "AcquireHomeButtonEventHandle"},
            {111, nullptr, "ActivateHomeButton"},
            {121, nullptr, "AcquireSleepButtonEventHandle"},
            {131, nullptr, "ActivateSleepButton"},
            {141, nullptr, "AcquireCaptureButtonEventHandle"},
            {151, nullptr, "ActivateCaptureButton"},
            {210, nullptr, "AcquireNfcDeviceUpdateEventHandle"},
            {211, nullptr, "GetNpadsWithNfc"},
            {212, nullptr, "AcquireNfcActivateEventHandle"},
            {213, nullptr, "ActivateNfc"},
            {214, nullptr, "GetXcdHandleForNpadWithNfc"},
            {215, nullptr, "IsNfcActivated"},
            {230, nullptr, "AcquireIrSensorEventHandle"},
            {231, nullptr, "ActivateIrSensor"},
            {301, nullptr, "ActivateNpadSystem"},
            {303, nullptr, "ApplyNpadSystemCommonPolicy"},
            {304, nullptr, "EnableAssigningSingleOnSlSrPress"},
            {305, nullptr, "DisableAssigningSingleOnSlSrPress"},
            {306, nullptr, "GetLastActiveNpad"},
            {307, nullptr, "GetNpadSystemExtStyle"},
            {308, nullptr, "ApplyNpadSystemCommonPolicyFull"},
            {309, nullptr, "GetNpadFullKeyGripColor"},
            {311, nullptr, "SetNpadPlayerLedBlinkingDevice"},
            {321, nullptr, "GetUniquePadsFromNpad"},
            {322, nullptr, "GetIrSensorState"},
            {323, nullptr, "GetXcdHandleForNpadWithIrSensor"},
            {500, nullptr, "SetAppletResourceUserId"},
            {501, nullptr, "RegisterAppletResourceUserId"},
            {502, nullptr, "UnregisterAppletResourceUserId"},
            {503, nullptr, "EnableAppletToGetInput"},
            {504, nullptr, "SetAruidValidForVibration"},
            {505, nullptr, "EnableAppletToGetSixAxisSensor"},
            {510, nullptr, "SetVibrationMasterVolume"},
            {511, nullptr, "GetVibrationMasterVolume"},
            {512, nullptr, "BeginPermitVibrationSession"},
            {513, nullptr, "EndPermitVibrationSession"},
            {520, nullptr, "EnableHandheldHids"},
            {521, nullptr, "DisableHandheldHids"},
            {540, nullptr, "AcquirePlayReportControllerUsageUpdateEvent"},
            {541, nullptr, "GetPlayReportControllerUsages"},
            {542, nullptr, "AcquirePlayReportRegisteredDeviceUpdateEvent"},
            {543, nullptr, "GetRegisteredDevicesOld"},
            {544, nullptr, "AcquireConnectionTriggerTimeoutEvent"},
            {545, nullptr, "SendConnectionTrigger"},
            {546, nullptr, "AcquireDeviceRegisteredEventForControllerSupport"},
            {547, nullptr, "GetAllowedBluetoothLinksCount"},
            {548, nullptr, "GetRegisteredDevices"},
            {700, nullptr, "ActivateUniquePad"},
            {702, nullptr, "AcquireUniquePadConnectionEventHandle"},
            {703, nullptr, "GetUniquePadIds"},
            {751, nullptr, "AcquireJoyDetachOnBluetoothOffEventHandle"},
            {800, nullptr, "ListSixAxisSensorHandles"},
            {801, nullptr, "IsSixAxisSensorUserCalibrationSupported"},
            {802, nullptr, "ResetSixAxisSensorCalibrationValues"},
            {803, nullptr, "StartSixAxisSensorUserCalibration"},
            {804, nullptr, "CancelSixAxisSensorUserCalibration"},
            {805, nullptr, "GetUniquePadBluetoothAddress"},
            {806, nullptr, "DisconnectUniquePad"},
            {807, nullptr, "GetUniquePadType"},
            {808, nullptr, "GetUniquePadInterface"},
            {809, nullptr, "GetUniquePadSerialNumber"},
            {810, nullptr, "GetUniquePadControllerNumber"},
            {811, nullptr, "GetSixAxisSensorUserCalibrationStage"},
            {821, nullptr, "StartAnalogStickManualCalibration"},
            {822, nullptr, "RetryCurrentAnalogStickManualCalibrationStage"},
            {823, nullptr, "CancelAnalogStickManualCalibration"},
            {824, nullptr, "ResetAnalogStickManualCalibration"},
            {825, nullptr, "GetAnalogStickState"},
            {826, nullptr, "GetAnalogStickManualCalibrationStage"},
            {827, nullptr, "IsAnalogStickButtonPressed"},
            {828, nullptr, "IsAnalogStickInReleasePosition"},
            {829, nullptr, "IsAnalogStickInCircumference"},
            {850, nullptr, "IsUsbFullKeyControllerEnabled"},
            {851, nullptr, "EnableUsbFullKeyController"},
            {852, nullptr, "IsUsbConnected"},
            {900, nullptr, "ActivateInputDetector"},
            {901, nullptr, "NotifyInputDetector"},
            {1000, nullptr, "InitializeFirmwareUpdate"},
            {1001, nullptr, "GetFirmwareVersion"},
            {1002, nullptr, "GetAvailableFirmwareVersion"},
            {1003, nullptr, "IsFirmwareUpdateAvailable"},
            {1004, nullptr, "CheckFirmwareUpdateRequired"},
            {1005, nullptr, "StartFirmwareUpdate"},
            {1006, nullptr, "AbortFirmwareUpdate"},
            {1007, nullptr, "GetFirmwareUpdateState"},
            {1008, nullptr, "ActivateAudioControl"},
            {1009, nullptr, "AcquireAudioControlEventHandle"},
            {1010, nullptr, "GetAudioControlStates"},
            {1011, nullptr, "DeactivateAudioControl"},
            {1050, nullptr, "IsSixAxisSensorAccurateUserCalibrationSupported"},
            {1051, nullptr, "StartSixAxisSensorAccurateUserCalibration"},
            {1052, nullptr, "CancelSixAxisSensorAccurateUserCalibration"},
            {1053, nullptr, "GetSixAxisSensorAccurateUserCalibrationState"},
            {1100, nullptr, "GetHidbusSystemServiceObject"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class HidTmp final : public ServiceFramework<HidTmp> {
public:
    explicit HidTmp() : ServiceFramework{"hid:tmp"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {0, nullptr, "GetConsoleSixAxisSensorCalibrationValues"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

class HidBus final : public ServiceFramework<HidBus> {
public:
    explicit HidBus() : ServiceFramework{"hidbus"} {
        // clang-format off
        static const FunctionInfo functions[] = {
            {1, nullptr, "GetBusHandle"},
            {2, nullptr, "IsExternalDeviceConnected"},
            {3, nullptr, "Initialize"},
            {4, nullptr, "Finalize"},
            {5, nullptr, "EnableExternalDevice"},
            {6, nullptr, "GetExternalDeviceId"},
            {7, nullptr, "SendCommandAsync"},
            {8, nullptr, "GetSendCommandAsynceResult"},
            {9, nullptr, "SetEventForSendCommandAsycResult"},
            {10, nullptr, "GetSharedMemoryHandle"},
            {11, nullptr, "EnableJoyPollingReceiveMode"},
            {12, nullptr, "DisableJoyPollingReceiveMode"},
            {13, nullptr, "GetPollingData"},
        };
        // clang-format on

        RegisterHandlers(functions);
    }
};

void ReloadInputDevices() {}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    std::make_shared<Hid>()->InstallAsService(service_manager);
    std::make_shared<HidBus>()->InstallAsService(service_manager);
    std::make_shared<HidDbg>()->InstallAsService(service_manager);
    std::make_shared<HidSys>()->InstallAsService(service_manager);
    std::make_shared<HidTmp>()->InstallAsService(service_manager);

    std::make_shared<IRS>()->InstallAsService(service_manager);
    std::make_shared<IRS_SYS>()->InstallAsService(service_manager);

    std::make_shared<XCD_SYS>()->InstallAsService(service_manager);
}

} // namespace Service::HID
