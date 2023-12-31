// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <array>

#include "common/common_types.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/types/touch_types.h"

namespace Core::HID {
class EmulatedConsole;
}

namespace Service::HID {
struct GestureSharedMemoryFormat;

class Gesture final : public ControllerBase {
public:
    explicit Gesture(Core::HID::HIDCore& hid_core_);
    ~Gesture() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

private:
    // Reads input from all available input engines
    void ReadTouchInput();

    // Returns true if gesture state needs to be updated
    bool ShouldUpdateGesture(const GestureProperties& gesture, f32 time_difference);

    // Updates the shared memory to the next state
    void UpdateGestureSharedMemory(GestureProperties& gesture, f32 time_difference);

    // Initializes new gesture
    void NewGesture(GestureProperties& gesture, GestureType& type, GestureAttribute& attributes);

    // Updates existing gesture state
    void UpdateExistingGesture(GestureProperties& gesture, GestureType& type, f32 time_difference);

    // Terminates exiting gesture
    void EndGesture(GestureProperties& gesture, GestureProperties& last_gesture_props,
                    GestureType& type, GestureAttribute& attributes, f32 time_difference);

    // Set current event to a tap event
    void SetTapEvent(GestureProperties& gesture, GestureProperties& last_gesture_props,
                     GestureType& type, GestureAttribute& attributes);

    // Calculates and set the extra parameters related to a pan event
    void UpdatePanEvent(GestureProperties& gesture, GestureProperties& last_gesture_props,
                        GestureType& type, f32 time_difference);

    // Terminates the pan event
    void EndPanEvent(GestureProperties& gesture, GestureProperties& last_gesture_props,
                     GestureType& type, f32 time_difference);

    // Set current event to a swipe event
    void SetSwipeEvent(GestureProperties& gesture, GestureProperties& last_gesture_props,
                       GestureType& type);

    // Retrieves the last gesture entry, as indicated by shared memory indices.
    [[nodiscard]] const GestureState& GetLastGestureEntry() const;

    // Returns the average distance, angle and middle point of the active fingers
    GestureProperties GetGestureProperties();

    GestureState next_state{};
    GestureSharedMemoryFormat* shared_memory;
    Core::HID::EmulatedConsole* console = nullptr;

    std::array<Core::HID::TouchFinger, MAX_POINTS> fingers{};
    GestureProperties last_gesture{};
    s64 last_update_timestamp{};
    s64 last_tap_timestamp{};
    f32 last_pan_time_difference{};
    bool force_update{false};
    bool enable_press_and_tap{false};
};
} // namespace Service::HID
