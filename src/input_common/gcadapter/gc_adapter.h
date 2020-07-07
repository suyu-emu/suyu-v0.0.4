// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once
#include <algorithm>
#include <functional>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <libusb.h>
#include "common/common_types.h"
#include "common/threadsafe_queue.h"

namespace GCAdapter {

enum {
    PAD_USE_ORIGIN = 0x0080,
    PAD_GET_ORIGIN = 0x2000,
    PAD_ERR_STATUS = 0x8000,
};

enum class PadButton {
    PAD_BUTTON_LEFT = 0x0001,
    PAD_BUTTON_RIGHT = 0x0002,
    PAD_BUTTON_DOWN = 0x0004,
    PAD_BUTTON_UP = 0x0008,
    PAD_TRIGGER_Z = 0x0010,
    PAD_TRIGGER_R = 0x0020,
    PAD_TRIGGER_L = 0x0040,
    PAD_BUTTON_A = 0x0100,
    PAD_BUTTON_B = 0x0200,
    PAD_BUTTON_X = 0x0400,
    PAD_BUTTON_Y = 0x0800,
    PAD_BUTTON_START = 0x1000,
    // Below is for compatibility with "AxisButton" type
    PAD_STICK = 0x2000,
};

extern const std::array<PadButton, 12> PadButtonArray;

enum class PadAxes : u8 {
    StickX,
    StickY,
    SubstickX,
    SubstickY,
    TriggerLeft,
    TriggerRight,
    Undefined,
};

struct GCPadStatus {
    u16 button{};       // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
    u8 stick_x{};       // 0 <= stick_x       <= 255
    u8 stick_y{};       // 0 <= stick_y       <= 255
    u8 substick_x{};    // 0 <= substick_x    <= 255
    u8 substick_y{};    // 0 <= substick_y    <= 255
    u8 trigger_left{};  // 0 <= trigger_left  <= 255
    u8 trigger_right{}; // 0 <= trigger_right <= 255

    static constexpr u8 MAIN_STICK_CENTER_X = 0x80;
    static constexpr u8 MAIN_STICK_CENTER_Y = 0x80;
    static constexpr u8 MAIN_STICK_RADIUS = 0x7f;
    static constexpr u8 C_STICK_CENTER_X = 0x80;
    static constexpr u8 C_STICK_CENTER_Y = 0x80;
    static constexpr u8 C_STICK_RADIUS = 0x7f;
    static constexpr u8 THRESHOLD = 10;

    // 256/4, at least a quarter press to count as a press. For polling mostly
    static constexpr u8 TRIGGER_THRESHOLD = 64;

    u8 port{};
    PadAxes axis{PadAxes::Undefined};
    u8 axis_value{255};
};

struct GCState {
    std::unordered_map<int, bool> buttons;
    std::unordered_map<int, u16> axes;
};

enum class ControllerTypes { None, Wired, Wireless };

enum {
    NO_ADAPTER_DETECTED = 0,
    ADAPTER_DETECTED = 1,
};

class Adapter {
public:
    /// Initialize the GC Adapter capture and read sequence
    Adapter();

    /// Close the adapter read thread and release the adapter
    ~Adapter();
    /// Used for polling
    void BeginConfiguration();
    void EndConfiguration();

    std::array<Common::SPSCQueue<GCPadStatus>, 4>& GetPadQueue();
    const std::array<Common::SPSCQueue<GCPadStatus>, 4>& GetPadQueue() const;

    std::array<GCState, 4>& GetPadState();
    const std::array<GCState, 4>& GetPadState() const;

private:
    GCPadStatus GetPadStatus(int port, const std::array<u8, 37>& adapter_payload);

    void PadToState(const GCPadStatus& pad, GCState& state);

    void Read();
    void ScanThreadFunc();
    /// Begin scanning for the GC Adapter.
    void StartScanThread();

    /// Stop scanning for the adapter
    void StopScanThread();

    /// Returns true if there is a device connected to port
    bool DeviceConnected(int port);

    /// Resets status of device connected to port
    void ResetDeviceType(int port);

    /// Returns true if we successfully gain access to GC Adapter
    bool CheckDeviceAccess(libusb_device* device);

    /// Captures GC Adapter endpoint address,
    void GetGCEndpoint(libusb_device* device);

    /// For shutting down, clear all data, join all threads, release usb
    void Reset();

    /// For use in initialization, querying devices to find the adapter
    void Setup();

    int current_status = NO_ADAPTER_DETECTED;
    libusb_device_handle* usb_adapter_handle = nullptr;
    std::array<ControllerTypes, 4> adapter_controllers_status{};

    std::mutex s_mutex;

    std::thread adapter_input_thread;
    bool adapter_thread_running;

    std::mutex initialization_mutex;
    std::thread detect_thread;
    bool detect_thread_running = false;

    libusb_context* libusb_ctx;

    u8 input_endpoint = 0;
    u8 output_endpoint = 0;

    bool configuring = false;

    std::array<Common::SPSCQueue<GCPadStatus>, 4> pad_queue;
    std::array<GCState, 4> state;
};

} // namespace GCAdapter
