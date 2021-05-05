// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/frontend/input.h"
#include "core/hle/service/hid/controllers/controller_base.h"

namespace Service::HID {
class Controller_Gesture final : public ControllerBase {
public:
    explicit Controller_Gesture(Core::System& system_);
    ~Controller_Gesture() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing, u8* data, size_t size) override;

    // Called when input devices should be loaded
    void OnLoadInputDevices() override;

private:
    static constexpr size_t MAX_FINGERS = 16;
    static constexpr size_t MAX_POINTS = 4;

    enum class TouchType : u32 {
        Idle,     // Nothing touching the screen
        Complete, // Unknown. End of touch?
        Cancel,   // Never triggered
        Touch,    // Pressing without movement
        Press,    // Never triggered
        Tap,      // Fast press then release
        Pan,      // All points moving together across the screen
        Swipe,    // Fast press movement and release of a single point
        Pinch,    // All points moving away/closer to the midpoint
        Rotate,   // All points rotating from the midpoint
    };

    enum class Direction : u32 {
        None,
        Left,
        Up,
        Right,
        Down,
    };

    struct Attribute {
        union {
            u32_le raw{};

            BitField<0, 1, u32> is_new_touch;
            BitField<1, 1, u32> is_double_tap;
        };
    };
    static_assert(sizeof(Attribute) == 4, "Attribute is an invalid size");

    struct Points {
        s32_le x;
        s32_le y;
    };
    static_assert(sizeof(Points) == 8, "Points is an invalid size");

    struct GestureState {
        s64_le sampling_number;
        s64_le sampling_number2;

        s64_le detection_count;
        TouchType type;
        Direction dir;
        s32_le x;
        s32_le y;
        s32_le delta_x;
        s32_le delta_y;
        f32 vel_x;
        f32 vel_y;
        Attribute attributes;
        u32 scale;
        u32 rotation_angle;
        s32_le point_count;
        std::array<Points, 4> points;
    };
    static_assert(sizeof(GestureState) == 0x68, "GestureState is an invalid size");

    struct SharedMemory {
        CommonHeader header;
        std::array<GestureState, 17> gesture_states;
    };
    static_assert(sizeof(SharedMemory) == 0x708, "SharedMemory is an invalid size");

    struct Finger {
        f32 x{};
        f32 y{};
        bool pressed{};
    };

    struct GestureProperties {
        std::array<Points, MAX_POINTS> points{};
        std::size_t active_points{};
        Points mid_point{};
        s64_le detection_count{};
        u64_le delta_time{};
        float average_distance{};
        float angle{};
    };

    // Returns an unused finger id, if there is no fingers avaliable MAX_FINGERS will be returned
    std::optional<size_t> GetUnusedFingerID() const;

    /** If the touch is new it tries to assing a new finger id, if there is no fingers avaliable no
     * changes will be made. Updates the coordinates if the finger id it's already set. If the touch
     * ends delays the output by one frame to set the end_touch flag before finally freeing the
     * finger id */
    size_t UpdateTouchInputEvent(const std::tuple<float, float, bool>& touch_input,
                                 size_t finger_id);

    // Returns the average distance, angle and middle point of the active fingers
    GestureProperties GetGestureProperties();

    SharedMemory shared_memory{};
    std::unique_ptr<Input::TouchDevice> touch_mouse_device;
    std::unique_ptr<Input::TouchDevice> touch_udp_device;
    std::unique_ptr<Input::TouchDevice> touch_btn_device;
    std::array<size_t, MAX_FINGERS> mouse_finger_id;
    std::array<size_t, MAX_FINGERS> keyboard_finger_id;
    std::array<size_t, MAX_FINGERS> udp_finger_id;
    std::array<Finger, MAX_POINTS> fingers;
    GestureProperties last_gesture;
};
} // namespace Service::HID
