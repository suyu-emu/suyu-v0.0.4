// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <chrono>
#include <map>
#include <optional>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Settings {

namespace NativeButton {
enum Values {
    A,
    B,
    X,
    Y,
    LStick,
    RStick,
    L,
    R,
    ZL,
    ZR,
    Plus,
    Minus,

    DLeft,
    DUp,
    DRight,
    DDown,

    LStick_Left,
    LStick_Up,
    LStick_Right,
    LStick_Down,

    RStick_Left,
    RStick_Up,
    RStick_Right,
    RStick_Down,

    SL,
    SR,

    Home,
    Screenshot,

    NumButtons,
};

constexpr int BUTTON_HID_BEGIN = A;
constexpr int BUTTON_NS_BEGIN = Home;

constexpr int BUTTON_HID_END = BUTTON_NS_BEGIN;
constexpr int BUTTON_NS_END = NumButtons;

constexpr int NUM_BUTTONS_HID = BUTTON_HID_END - BUTTON_HID_BEGIN;
constexpr int NUM_BUTTONS_NS = BUTTON_NS_END - BUTTON_NS_BEGIN;

extern const std::array<const char*, NumButtons> mapping;

} // namespace NativeButton

namespace NativeAnalog {
enum Values {
    LStick,
    RStick,

    NumAnalogs,
};

constexpr int STICK_HID_BEGIN = LStick;
constexpr int STICK_HID_END = NumAnalogs;
constexpr int NUM_STICKS_HID = NumAnalogs;

extern const std::array<const char*, NumAnalogs> mapping;
} // namespace NativeAnalog

namespace NativeMouseButton {
enum Values {
    Left,
    Right,
    Middle,
    Forward,
    Back,

    NumMouseButtons,
};

constexpr int MOUSE_HID_BEGIN = Left;
constexpr int MOUSE_HID_END = NumMouseButtons;
constexpr int NUM_MOUSE_HID = NumMouseButtons;

extern const std::array<const char*, NumMouseButtons> mapping;
} // namespace NativeMouseButton

namespace NativeKeyboard {
enum Keys {
    None,
    Error,

    A = 4,
    B,
    C,
    D,
    E,
    F,
    G,
    H,
    I,
    J,
    K,
    L,
    M,
    N,
    O,
    P,
    Q,
    R,
    S,
    T,
    U,
    V,
    W,
    X,
    Y,
    Z,
    N1,
    N2,
    N3,
    N4,
    N5,
    N6,
    N7,
    N8,
    N9,
    N0,
    Enter,
    Escape,
    Backspace,
    Tab,
    Space,
    Minus,
    Equal,
    LeftBrace,
    RightBrace,
    Backslash,
    Tilde,
    Semicolon,
    Apostrophe,
    Grave,
    Comma,
    Dot,
    Slash,
    CapsLockKey,

    F1,
    F2,
    F3,
    F4,
    F5,
    F6,
    F7,
    F8,
    F9,
    F10,
    F11,
    F12,

    SystemRequest,
    ScrollLockKey,
    Pause,
    Insert,
    Home,
    PageUp,
    Delete,
    End,
    PageDown,
    Right,
    Left,
    Down,
    Up,

    NumLockKey,
    KPSlash,
    KPAsterisk,
    KPMinus,
    KPPlus,
    KPEnter,
    KP1,
    KP2,
    KP3,
    KP4,
    KP5,
    KP6,
    KP7,
    KP8,
    KP9,
    KP0,
    KPDot,

    Key102,
    Compose,
    Power,
    KPEqual,

    F13,
    F14,
    F15,
    F16,
    F17,
    F18,
    F19,
    F20,
    F21,
    F22,
    F23,
    F24,

    Open,
    Help,
    Properties,
    Front,
    Stop,
    Repeat,
    Undo,
    Cut,
    Copy,
    Paste,
    Find,
    Mute,
    VolumeUp,
    VolumeDown,
    CapsLockActive,
    NumLockActive,
    ScrollLockActive,
    KPComma,

    KPLeftParenthesis,
    KPRightParenthesis,

    LeftControlKey = 0xE0,
    LeftShiftKey,
    LeftAltKey,
    LeftMetaKey,
    RightControlKey,
    RightShiftKey,
    RightAltKey,
    RightMetaKey,

    MediaPlayPause,
    MediaStopCD,
    MediaPrevious,
    MediaNext,
    MediaEject,
    MediaVolumeUp,
    MediaVolumeDown,
    MediaMute,
    MediaWebsite,
    MediaBack,
    MediaForward,
    MediaStop,
    MediaFind,
    MediaScrollUp,
    MediaScrollDown,
    MediaEdit,
    MediaSleep,
    MediaCoffee,
    MediaRefresh,
    MediaCalculator,

    NumKeyboardKeys,
};

static_assert(NumKeyboardKeys == 0xFC, "Incorrect number of keyboard keys.");

enum Modifiers {
    LeftControl,
    LeftShift,
    LeftAlt,
    LeftMeta,
    RightControl,
    RightShift,
    RightAlt,
    RightMeta,
    CapsLock,
    ScrollLock,
    NumLock,

    NumKeyboardMods,
};

constexpr int KEYBOARD_KEYS_HID_BEGIN = None;
constexpr int KEYBOARD_KEYS_HID_END = NumKeyboardKeys;
constexpr int NUM_KEYBOARD_KEYS_HID = NumKeyboardKeys;

constexpr int KEYBOARD_MODS_HID_BEGIN = LeftControl;
constexpr int KEYBOARD_MODS_HID_END = NumKeyboardMods;
constexpr int NUM_KEYBOARD_MODS_HID = NumKeyboardMods;

} // namespace NativeKeyboard

using ButtonsRaw = std::array<std::string, NativeButton::NumButtons>;
using AnalogsRaw = std::array<std::string, NativeAnalog::NumAnalogs>;
using MouseButtonsRaw = std::array<std::string, NativeMouseButton::NumMouseButtons>;
using KeyboardKeysRaw = std::array<std::string, NativeKeyboard::NumKeyboardKeys>;
using KeyboardModsRaw = std::array<std::string, NativeKeyboard::NumKeyboardMods>;

constexpr u32 JOYCON_BODY_NEON_RED = 0xFF3C28;
constexpr u32 JOYCON_BUTTONS_NEON_RED = 0x1E0A0A;
constexpr u32 JOYCON_BODY_NEON_BLUE = 0x0AB9E6;
constexpr u32 JOYCON_BUTTONS_NEON_BLUE = 0x001E1E;

enum class ControllerType {
    ProController,
    DualJoycon,
    RightJoycon,
    LeftJoycon,
};

struct PlayerInput {
    bool connected;
    ControllerType type;
    ButtonsRaw buttons;
    AnalogsRaw analogs;

    u32 body_color_right;
    u32 button_color_right;
    u32 body_color_left;
    u32 button_color_left;
};

struct TouchscreenInput {
    bool enabled;
    std::string device;

    u32 finger;
    u32 diameter_x;
    u32 diameter_y;
    u32 rotation_angle;
};

enum class RendererBackend {
    OpenGL = 0,
    Vulkan = 1,
};

enum class GPUAccuracy : u32 {
    Normal = 0,
    High = 1,
    Extreme = 2,
};

enum class CPUAccuracy {
    Accurate = 0,
    DebugMode = 1,
};

extern bool configuring_global;

template <typename Type>
class Setting final {
public:
    Setting() = default;
    explicit Setting(Type val) : global{val} {}
    ~Setting() = default;
    void SetGlobal(bool to_global) {
        use_global = to_global;
    }
    bool UsingGlobal() const {
        return use_global;
    }
    Type GetValue(bool need_global = false) const {
        if (use_global || need_global) {
            return global;
        }
        return local;
    }
    void SetValue(const Type& value) {
        if (use_global) {
            global = value;
        } else {
            local = value;
        }
    }

private:
    bool use_global = true;
    Type global{};
    Type local{};
};

struct Values {
    // Audio
    std::string audio_device_id;
    std::string sink_id;
    bool audio_muted;
    Setting<bool> enable_audio_stretching;
    Setting<float> volume;

    // Core
    Setting<bool> use_multi_core;

    // Cpu
    CPUAccuracy cpu_accuracy;

    bool cpuopt_page_tables;
    bool cpuopt_block_linking;
    bool cpuopt_return_stack_buffer;
    bool cpuopt_fast_dispatcher;
    bool cpuopt_context_elimination;
    bool cpuopt_const_prop;
    bool cpuopt_misc_ir;
    bool cpuopt_reduce_misalign_checks;

    // Renderer
    Setting<RendererBackend> renderer_backend;
    bool renderer_debug;
    Setting<int> vulkan_device;

    Setting<u16> resolution_factor = Setting(static_cast<u16>(1));
    Setting<int> aspect_ratio;
    Setting<int> max_anisotropy;
    Setting<bool> use_frame_limit;
    Setting<u16> frame_limit;
    Setting<bool> use_disk_shader_cache;
    Setting<GPUAccuracy> gpu_accuracy;
    Setting<bool> use_asynchronous_gpu_emulation;
    Setting<bool> use_vsync;
    Setting<bool> use_assembly_shaders;
    Setting<bool> use_asynchronous_shaders;
    Setting<bool> force_30fps_mode;
    Setting<bool> use_fast_gpu_time;

    Setting<float> bg_red;
    Setting<float> bg_green;
    Setting<float> bg_blue;

    // System
    Setting<std::optional<u32>> rng_seed;
    // Measured in seconds since epoch
    Setting<std::optional<std::chrono::seconds>> custom_rtc;
    // Set on game boot, reset on stop. Seconds difference between current time and `custom_rtc`
    std::chrono::seconds custom_rtc_differential;

    s32 current_user;
    Setting<s32> language_index;
    Setting<s32> region_index;
    Setting<s32> time_zone_index;
    Setting<s32> sound_index;

    // Controls
    std::array<PlayerInput, 10> players;

    bool mouse_enabled;
    std::string mouse_device;
    MouseButtonsRaw mouse_buttons;

    bool keyboard_enabled;
    KeyboardKeysRaw keyboard_keys;
    KeyboardModsRaw keyboard_mods;

    bool debug_pad_enabled;
    ButtonsRaw debug_pad_buttons;
    AnalogsRaw debug_pad_analogs;

    std::string motion_device;
    TouchscreenInput touchscreen;
    std::atomic_bool is_device_reload_pending{true};
    std::string udp_input_address;
    u16 udp_input_port;
    u8 udp_pad_index;

    bool use_docked_mode;

    // Data Storage
    bool use_virtual_sd;
    bool gamecard_inserted;
    bool gamecard_current_game;
    std::string gamecard_path;

    // Debugging
    bool record_frame_times;
    bool use_gdbstub;
    u16 gdbstub_port;
    std::string program_args;
    bool dump_exefs;
    bool dump_nso;
    bool reporting_services;
    bool quest_flag;
    bool disable_macro_jit;

    // Misceallaneous
    std::string log_filter;
    bool use_dev_keys;

    // Services
    std::string bcat_backend;
    bool bcat_boxcat_local;

    // WebService
    bool enable_telemetry;
    std::string web_api_url;
    std::string yuzu_username;
    std::string yuzu_token;

    // Add-Ons
    std::map<u64, std::vector<std::string>> disabled_addons;
} extern values;

float Volume();

bool IsGPULevelExtreme();
bool IsGPULevelHigh();

std::string GetTimeZoneString();

void Apply();
void LogSettings();

// Restore the global state of all applicable settings in the Values struct
void RestoreGlobalState();

// Fixes settings that are known to cause issues with the emulator
void Sanitize();

} // namespace Settings
