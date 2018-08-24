// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "core/hle/service/service.h"

namespace Service::HID {

// Begin enums and output structs

constexpr u32 HID_NUM_ENTRIES = 17;
constexpr u32 HID_NUM_LAYOUTS = 7;
constexpr s32 HID_JOYSTICK_MAX = 0x8000;
constexpr s32 HID_JOYSTICK_MIN = -0x8000;

constexpr u32 JOYCON_BODY_NEON_RED = 0xFF3C28;
constexpr u32 JOYCON_BUTTONS_NEON_RED = 0x1E0A0A;
constexpr u32 JOYCON_BODY_NEON_BLUE = 0x0AB9E6;
constexpr u32 JOYCON_BUTTONS_NEON_BLUE = 0x001E1E;

enum ControllerType : u32 {
    ControllerType_ProController = 1 << 0,
    ControllerType_Handheld = 1 << 1,
    ControllerType_JoyconPair = 1 << 2,
    ControllerType_JoyconLeft = 1 << 3,
    ControllerType_JoyconRight = 1 << 4,
};

enum ControllerLayoutType : u32 {
    Layout_ProController = 0, // Pro Controller or HID gamepad
    Layout_Handheld = 1,      // Two Joy-Con docked to rails
    Layout_Single = 2, // Horizontal single Joy-Con or pair of Joy-Con, adjusted for orientation
    Layout_Left = 3,   // Only raw left Joy-Con state, no orientation adjustment
    Layout_Right = 4,  // Only raw right Joy-Con state, no orientation adjustment
    Layout_DefaultDigital = 5, // Same as next, but sticks have 8-direction values only
    Layout_Default = 6, // Safe default, single Joy-Con have buttons/sticks rotated for orientation
};

enum ControllerColorDescription {
    ColorDesc_ColorsNonexistent = 1 << 1,
};

enum ControllerConnectionState {
    ConnectionState_Connected = 1 << 0,
    ConnectionState_Wired = 1 << 1,
};

enum ControllerJoystick {
    Joystick_Left = 0,
    Joystick_Right = 1,
};

enum ControllerID {
    Controller_Player1 = 0,
    Controller_Player2 = 1,
    Controller_Player3 = 2,
    Controller_Player4 = 3,
    Controller_Player5 = 4,
    Controller_Player6 = 5,
    Controller_Player7 = 6,
    Controller_Player8 = 7,
    Controller_Handheld = 8,
    Controller_Unknown = 9,
};

// End enums and output structs

// Begin UnkInput3

struct UnkInput3Header {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(UnkInput3Header) == 0x20, "HID UnkInput3 header structure has incorrect size");

struct UnkInput3Entry {
    u64 timestamp;
    u64 timestamp_2;
    u64 unk_8;
    u64 unk_10;
    u64 unk_18;
};
static_assert(sizeof(UnkInput3Entry) == 0x28, "HID UnkInput3 entry structure has incorrect size");

struct UnkInput3 {
    UnkInput3Header header;
    std::array<UnkInput3Entry, 17> entries;
    std::array<u8, 0x138> padding;
};
static_assert(sizeof(UnkInput3) == 0x400, "HID UnkInput3 structure has incorrect size");

// End UnkInput3

// Begin TouchScreen

struct TouchScreenHeader {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
    u64 timestamp;
};
static_assert(sizeof(TouchScreenHeader) == 0x28,
              "HID touch screen header structure has incorrect size");

struct TouchScreenEntryHeader {
    u64 timestamp;
    u64 num_touches;
};
static_assert(sizeof(TouchScreenEntryHeader) == 0x10,
              "HID touch screen entry header structure has incorrect size");

struct TouchScreenEntryTouch {
    u64 timestamp;
    u32 padding;
    u32 touch_index;
    u32 x;
    u32 y;
    u32 diameter_x;
    u32 diameter_y;
    u32 angle;
    u32 padding_2;
};
static_assert(sizeof(TouchScreenEntryTouch) == 0x28,
              "HID touch screen touch structure has incorrect size");

struct TouchScreenEntry {
    TouchScreenEntryHeader header;
    std::array<TouchScreenEntryTouch, 16> touches;
    u64 unk;
};
static_assert(sizeof(TouchScreenEntry) == 0x298,
              "HID touch screen entry structure has incorrect size");

struct TouchScreen {
    TouchScreenHeader header;
    std::array<TouchScreenEntry, 17> entries;
    std::array<u8, 0x3c0> padding;
};
static_assert(sizeof(TouchScreen) == 0x3000, "HID touch screen structure has incorrect size");

// End TouchScreen

// Begin Mouse

struct MouseHeader {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(MouseHeader) == 0x20, "HID mouse header structure has incorrect size");

struct MouseButtonState {
    union {
        u64 hex{};

        // Buttons
        BitField<0, 1, u64> left;
        BitField<1, 1, u64> right;
        BitField<2, 1, u64> middle;
        BitField<3, 1, u64> forward;
        BitField<4, 1, u64> back;
    };
};

struct MouseEntry {
    u64 timestamp;
    u64 timestamp_2;
    u32 x;
    u32 y;
    u32 velocity_x;
    u32 velocity_y;
    u32 scroll_velocity_x;
    u32 scroll_velocity_y;
    MouseButtonState buttons;
};
static_assert(sizeof(MouseEntry) == 0x30, "HID mouse entry structure has incorrect size");

struct Mouse {
    MouseHeader header;
    std::array<MouseEntry, 17> entries;
    std::array<u8, 0xB0> padding;
};
static_assert(sizeof(Mouse) == 0x400, "HID mouse structure has incorrect size");

// End Mouse

// Begin Keyboard

struct KeyboardHeader {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(KeyboardHeader) == 0x20, "HID keyboard header structure has incorrect size");

struct KeyboardModifierKeyState {
    union {
        u64 hex{};

        // Buttons
        BitField<0, 1, u64> lctrl;
        BitField<1, 1, u64> lshift;
        BitField<2, 1, u64> lalt;
        BitField<3, 1, u64> lmeta;
        BitField<4, 1, u64> rctrl;
        BitField<5, 1, u64> rshift;
        BitField<6, 1, u64> ralt;
        BitField<7, 1, u64> rmeta;
        BitField<8, 1, u64> capslock;
        BitField<9, 1, u64> scrolllock;
        BitField<10, 1, u64> numlock;
    };
};

struct KeyboardEntry {
    u64 timestamp;
    u64 timestamp_2;
    KeyboardModifierKeyState modifier;
    u32 keys[8];
};
static_assert(sizeof(KeyboardEntry) == 0x38, "HID keyboard entry structure has incorrect size");

struct Keyboard {
    KeyboardHeader header;
    std::array<KeyboardEntry, 17> entries;
    std::array<u8, 0x28> padding;
};
static_assert(sizeof(Keyboard) == 0x400, "HID keyboard structure has incorrect size");

// End Keyboard

// Begin UnkInput1

struct UnkInput1Header {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(UnkInput1Header) == 0x20, "HID UnkInput1 header structure has incorrect size");

struct UnkInput1Entry {
    u64 timestamp;
    u64 timestamp_2;
    u64 unk_8;
    u64 unk_10;
    u64 unk_18;
};
static_assert(sizeof(UnkInput1Entry) == 0x28, "HID UnkInput1 entry structure has incorrect size");

struct UnkInput1 {
    UnkInput1Header header;
    std::array<UnkInput1Entry, 17> entries;
    std::array<u8, 0x138> padding;
};
static_assert(sizeof(UnkInput1) == 0x400, "HID UnkInput1 structure has incorrect size");

// End UnkInput1

// Begin UnkInput2

struct UnkInput2Header {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(UnkInput2Header) == 0x20, "HID UnkInput2 header structure has incorrect size");

struct UnkInput2 {
    UnkInput2Header header;
    std::array<u8, 0x1E0> padding;
};
static_assert(sizeof(UnkInput2) == 0x200, "HID UnkInput2 structure has incorrect size");

// End UnkInput2

// Begin Controller

struct ControllerMAC {
    u64 timestamp;
    std::array<u8, 0x8> mac;
    u64 unk;
    u64 timestamp_2;
};
static_assert(sizeof(ControllerMAC) == 0x20, "HID controller MAC structure has incorrect size");

struct ControllerHeader {
    u32 type;
    u32 is_half;
    u32 single_colors_descriptor;
    u32 single_color_body;
    u32 single_color_buttons;
    u32 split_colors_descriptor;
    u32 left_color_body;
    u32 left_color_buttons;
    u32 right_color_body;
    u32 right_color_buttons;
};
static_assert(sizeof(ControllerHeader) == 0x28,
              "HID controller header structure has incorrect size");

struct ControllerLayoutHeader {
    u64 timestamp_ticks;
    u64 num_entries;
    u64 latest_entry;
    u64 max_entry_index;
};
static_assert(sizeof(ControllerLayoutHeader) == 0x20,
              "HID controller layout header structure has incorrect size");

struct ControllerPadState {
    union {
        u64 hex{};

        // Buttons
        BitField<0, 1, u64> a;
        BitField<1, 1, u64> b;
        BitField<2, 1, u64> x;
        BitField<3, 1, u64> y;
        BitField<4, 1, u64> lstick;
        BitField<5, 1, u64> rstick;
        BitField<6, 1, u64> l;
        BitField<7, 1, u64> r;
        BitField<8, 1, u64> zl;
        BitField<9, 1, u64> zr;
        BitField<10, 1, u64> plus;
        BitField<11, 1, u64> minus;

        // D-pad buttons
        BitField<12, 1, u64> dleft;
        BitField<13, 1, u64> dup;
        BitField<14, 1, u64> dright;
        BitField<15, 1, u64> ddown;

        // Left stick directions
        BitField<16, 1, u64> lstick_left;
        BitField<17, 1, u64> lstick_up;
        BitField<18, 1, u64> lstick_right;
        BitField<19, 1, u64> lstick_down;

        // Right stick directions
        BitField<20, 1, u64> rstick_left;
        BitField<21, 1, u64> rstick_up;
        BitField<22, 1, u64> rstick_right;
        BitField<23, 1, u64> rstick_down;

        BitField<24, 1, u64> sl;
        BitField<25, 1, u64> sr;
    };
};

struct ControllerInputEntry {
    u64 timestamp;
    u64 timestamp_2;
    ControllerPadState buttons;
    s32 joystick_left_x;
    s32 joystick_left_y;
    s32 joystick_right_x;
    s32 joystick_right_y;
    u64 connection_state;
};
static_assert(sizeof(ControllerInputEntry) == 0x30,
              "HID controller input entry structure has incorrect size");

struct ControllerLayout {
    ControllerLayoutHeader header;
    std::array<ControllerInputEntry, 17> entries;
};
static_assert(sizeof(ControllerLayout) == 0x350,
              "HID controller layout structure has incorrect size");

struct Controller {
    ControllerHeader header;
    std::array<ControllerLayout, HID_NUM_LAYOUTS> layouts;
    std::array<u8, 0x2a70> unk_1;
    ControllerMAC mac_left;
    ControllerMAC mac_right;
    std::array<u8, 0xdf8> unk_2;
};
static_assert(sizeof(Controller) == 0x5000, "HID controller structure has incorrect size");

// End Controller

struct SharedMemory {
    UnkInput3 unk_input_3;
    TouchScreen touchscreen;
    Mouse mouse;
    Keyboard keyboard;
    std::array<UnkInput1, 4> unk_input_1;
    std::array<UnkInput2, 3> unk_input_2;
    std::array<u8, 0x800> unk_section_8;
    std::array<u8, 0x4000> controller_serials;
    std::array<Controller, 10> controllers;
    std::array<u8, 0x4600> unk_section_9;
};
static_assert(sizeof(SharedMemory) == 0x40000, "HID Shared Memory structure has incorrect size");

/// Reload input devices. Used when input configuration changed
void ReloadInputDevices();

/// Registers all HID services with the specified service manager.
void InstallInterfaces(SM::ServiceManager& service_manager);

} // namespace Service::HID
