#pragma once
#include <algorithm>
#include <libusb.h>
#include <mutex>
#include <functional>
#include "common/common_types.h"


enum {
    PAD_USE_ORIGIN = 0x0080,
    PAD_GET_ORIGIN = 0x2000,
    PAD_ERR_STATUS = 0x8000,
};

enum PadButton {
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

enum PadAxes { STICK_X, STICK_Y, SUBSTICK_X, SUBSTICK_Y, TRIGGER_LEFT, TRIGGER_RIGHT };

struct GCPadStatus {
    u16 button;      // Or-ed PAD_BUTTON_* and PAD_TRIGGER_* bits
    u8 stickX;       // 0 <= stickX       <= 255
    u8 stickY;       // 0 <= stickY       <= 255
    u8 substickX;    // 0 <= substickX    <= 255
    u8 substickY;    // 0 <= substickY    <= 255
    u8 triggerLeft;  // 0 <= triggerLeft  <= 255
    u8 triggerRight; // 0 <= triggerRight <= 255
    bool isConnected{true};

    static const u8 MAIN_STICK_CENTER_X = 0x80;
    static const u8 MAIN_STICK_CENTER_Y = 0x80;
    static const u8 MAIN_STICK_RADIUS = 0x7f;
    static const u8 C_STICK_CENTER_X = 0x80;
    static const u8 C_STICK_CENTER_Y = 0x80;
    static const u8 C_STICK_RADIUS = 0x7f;

    static const u8 TRIGGER_CENTER = 20;
    static const u8 THRESHOLD = 10;
    u8 port;
    u8 axis_which = 255;
    u8 axis_value = 255;
};

struct GCState {
    std::unordered_map<int, bool> buttons;
    std::unordered_map<int, u16> axes;
};


namespace GCAdapter {
enum ControllerTypes {
    CONTROLLER_NONE = 0,
    CONTROLLER_WIRED = 1,
    CONTROLLER_WIRELESS = 2
};

enum {
    NO_ADAPTER_DETECTED = 0,
    ADAPTER_DETECTED = 1,
};

// Current adapter status: detected/not detected/in error (holds the error code)
static int current_status = NO_ADAPTER_DETECTED;

GCPadStatus CheckStatus(int port, u8 adapter_payload[37]);
/// Initialize the GC Adapter capture and read sequence
void Init();

/// Close the adapter read thread and release the adapter
void Shutdown();

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

/// Used for polling
void BeginConfiguration();

void EndConfiguration();

} // end of namespace GCAdapter
