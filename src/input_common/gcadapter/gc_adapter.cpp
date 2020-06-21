// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.
//*
#include "common/logging/log.h"
#include "common/threadsafe_queue.h"
#include "input_common/gcadapter/gc_adapter.h"

Common::SPSCQueue<GCPadStatus> pad_queue[4];
struct GCState state[4];

namespace GCAdapter {

static libusb_device_handle* usb_adapter_handle = nullptr;
static u8 adapter_controllers_status[4] = {
    ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE,
    ControllerTypes::CONTROLLER_NONE, ControllerTypes::CONTROLLER_NONE};

static std::mutex s_mutex;

static std::thread adapter_input_thread;
static bool adapter_thread_running;

static std::mutex initialization_mutex;
static std::thread detect_thread;
static bool detect_thread_running = false;

static libusb_context* libusb_ctx;

static u8 input_endpoint = 0;

static bool configuring = false;

GCPadStatus CheckStatus(int port, u8 adapter_payload[37]) {
    GCPadStatus pad = {};
    bool get_origin = false;

    u8 type = adapter_payload[1 + (9 * port)] >> 4;
    if (type)
        get_origin = true;

    adapter_controllers_status[port] = type;

    if (adapter_controllers_status[port] != ControllerTypes::CONTROLLER_NONE) {
        u8 b1 = adapter_payload[1 + (9 * port) + 1];
        u8 b2 = adapter_payload[1 + (9 * port) + 2];

        if (b1 & (1 << 0))
            pad.button |= PAD_BUTTON_A;
        if (b1 & (1 << 1))
            pad.button |= PAD_BUTTON_B;
        if (b1 & (1 << 2))
            pad.button |= PAD_BUTTON_X;
        if (b1 & (1 << 3))
            pad.button |= PAD_BUTTON_Y;

        if (b1 & (1 << 4))
            pad.button |= PAD_BUTTON_LEFT;
        if (b1 & (1 << 5))
            pad.button |= PAD_BUTTON_RIGHT;
        if (b1 & (1 << 6))
            pad.button |= PAD_BUTTON_DOWN;
        if (b1 & (1 << 7))
            pad.button |= PAD_BUTTON_UP;

        if (b2 & (1 << 0))
            pad.button |= PAD_BUTTON_START;
        if (b2 & (1 << 1))
            pad.button |= PAD_TRIGGER_Z;
        if (b2 & (1 << 2))
            pad.button |= PAD_TRIGGER_R;
        if (b2 & (1 << 3))
            pad.button |= PAD_TRIGGER_L;

        if (get_origin)
            pad.button |= PAD_GET_ORIGIN;

        pad.stickX = adapter_payload[1 + (9 * port) + 3];
        pad.stickY = adapter_payload[1 + (9 * port) + 4];
        pad.substickX = adapter_payload[1 + (9 * port) + 5];
        pad.substickY = adapter_payload[1 + (9 * port) + 6];
        pad.triggerLeft = adapter_payload[1 + (9 * port) + 7];
        pad.triggerRight = adapter_payload[1 + (9 * port) + 8];
    }
    return pad;
}

void PadToState(GCPadStatus pad, GCState& state) {
    //std::lock_guard lock{s_mutex};
    state.buttons.insert_or_assign(PAD_BUTTON_A, pad.button & PAD_BUTTON_A);
    state.buttons.insert_or_assign(PAD_BUTTON_B, pad.button & PAD_BUTTON_B);
    state.buttons.insert_or_assign(PAD_BUTTON_X, pad.button & PAD_BUTTON_X);
    state.buttons.insert_or_assign(PAD_BUTTON_Y, pad.button & PAD_BUTTON_Y);
    state.buttons.insert_or_assign(PAD_BUTTON_LEFT, pad.button & PAD_BUTTON_LEFT);
    state.buttons.insert_or_assign(PAD_BUTTON_RIGHT, pad.button & PAD_BUTTON_RIGHT);
    state.buttons.insert_or_assign(PAD_BUTTON_DOWN, pad.button & PAD_BUTTON_DOWN);
    state.buttons.insert_or_assign(PAD_BUTTON_UP, pad.button & PAD_BUTTON_UP);
    state.buttons.insert_or_assign(PAD_BUTTON_START, pad.button & PAD_BUTTON_START);
    state.buttons.insert_or_assign(PAD_TRIGGER_Z, pad.button & PAD_TRIGGER_Z);
    state.buttons.insert_or_assign(PAD_TRIGGER_L, pad.button & PAD_TRIGGER_L);
    state.buttons.insert_or_assign(PAD_TRIGGER_R, pad.button & PAD_TRIGGER_R);
    state.axes.insert_or_assign(STICK_X, pad.stickX);
    state.axes.insert_or_assign(STICK_Y, pad.stickY);
    state.axes.insert_or_assign(SUBSTICK_X, pad.substickX);
    state.axes.insert_or_assign(SUBSTICK_Y, pad.substickY);
    state.axes.insert_or_assign(TRIGGER_LEFT, pad.triggerLeft);
    state.axes.insert_or_assign(TRIGGER_RIGHT, pad.triggerRight);
}

static void Read() {
    LOG_INFO(Input, "GC Adapter Read() thread started");

    int payload_size_in;
    u8 adapter_payload[37];
    while (adapter_thread_running) {
        libusb_interrupt_transfer(usb_adapter_handle, input_endpoint, adapter_payload,
                                  sizeof(adapter_payload), &payload_size_in, 32);

        int payload_size = 0;
        u8 controller_payload_copy[37];

        {
            std::lock_guard<std::mutex> lk(s_mutex);
            std::copy(std::begin(adapter_payload), std::end(adapter_payload),
                      std::begin(controller_payload_copy));
            payload_size = payload_size_in;
        }
        
        GCPadStatus pad[4];
        if (payload_size != sizeof(controller_payload_copy) ||
            controller_payload_copy[0] != LIBUSB_DT_HID) {
            LOG_ERROR(Input, "error reading payload (size: %d, type: %02x)", payload_size,
                      controller_payload_copy[0]);
        } else {
            for (int i = 0; i < 4; i++)
                pad[i] = CheckStatus(i, controller_payload_copy);
        }
        for (int port = 0; port < 4; port++) {
            if (DeviceConnected(port) && configuring) {
                if (pad[port].button != PAD_GET_ORIGIN)
                    pad_queue[port].Push(pad[port]);

                // Accounting for a threshold here because of some controller variance
                if (pad[port].stickX > pad[port].MAIN_STICK_CENTER_X + pad[port].THRESHOLD ||
                    pad[port].stickX < pad[port].MAIN_STICK_CENTER_X - pad[port].THRESHOLD) {
                    pad[port].axis_which = STICK_X;
                    pad[port].axis_value = pad[port].stickX;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].stickY > pad[port].MAIN_STICK_CENTER_Y + pad[port].THRESHOLD ||
                    pad[port].stickY < pad[port].MAIN_STICK_CENTER_Y - pad[port].THRESHOLD) {
                    pad[port].axis_which = STICK_Y;
                    pad[port].axis_value = pad[port].stickY;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].substickX > pad[port].C_STICK_CENTER_X + pad[port].THRESHOLD ||
                    pad[port].substickX < pad[port].C_STICK_CENTER_X - pad[port].THRESHOLD) {
                    pad[port].axis_which = SUBSTICK_X;
                    pad[port].axis_value = pad[port].substickX;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].substickY > pad[port].C_STICK_CENTER_Y + pad[port].THRESHOLD ||
                    pad[port].substickY < pad[port].C_STICK_CENTER_Y - pad[port].THRESHOLD) {
                    pad[port].axis_which = SUBSTICK_Y;
                    pad[port].axis_value = pad[port].substickY;
                    pad_queue[port].Push(pad[port]);
                }
            }
            PadToState(pad[port], state[port]);
        }
        std::this_thread::yield();
    }
}

static void ScanThreadFunc() {
    LOG_INFO(Input, "GC Adapter scanning thread started");

    while (detect_thread_running) {
        if (usb_adapter_handle == nullptr) {
            std::lock_guard<std::mutex> lk(initialization_mutex);
            Setup();
        }
        Sleep(500);
    }
}

void Init() {

    if (usb_adapter_handle != nullptr)
        return;
    LOG_INFO(Input, "GC Adapter Initialization started");

    current_status = NO_ADAPTER_DETECTED;
    libusb_init(&libusb_ctx);

    StartScanThread();
}

void StartScanThread() {
    if (detect_thread_running)
        return;
    if (!libusb_ctx)
        return;

    detect_thread_running = true;
    detect_thread = std::thread(ScanThreadFunc);
}

void StopScanThread() {
    detect_thread.join();
}

static void Setup() {
    // Reset the error status in case the adapter gets unplugged
    if (current_status < 0)
        current_status = NO_ADAPTER_DETECTED;

    for (int i = 0; i < 4; i++)
        adapter_controllers_status[i] = ControllerTypes::CONTROLLER_NONE;
    
    libusb_device** devs; // pointer to list of connected usb devices

    int cnt = libusb_get_device_list(libusb_ctx, &devs); //get the list of devices

    for (int i = 0; i < cnt; i++) {
        if (CheckDeviceAccess(devs[i])) {
            // GC Adapter found, registering it
            GetGCEndpoint(devs[i]);
            break;
        }
    }
}

static bool CheckDeviceAccess(libusb_device* device) {
    libusb_device_descriptor desc;
    int ret = libusb_get_device_descriptor(device, &desc);
    if (ret) {
        // could not acquire the descriptor, no point in trying to use it.
        LOG_ERROR(Input, "libusb_get_device_descriptor failed with error: %d", ret);
        return false;
    }

    if (desc.idVendor != 0x057e || desc.idProduct != 0x0337) {
        // This isn’t the device we are looking for.
        return false;
    }
    ret = libusb_open(device, &usb_adapter_handle);

    if (ret == LIBUSB_ERROR_ACCESS) {
        LOG_ERROR(Input,
            "Yuzu can not gain access to this device: ID %04X:%04X.",
            desc.idVendor, desc.idProduct);
        return false;
    }
    if (ret) {
        LOG_ERROR(Input, "libusb_open failed to open device with error = %d", ret);
        return false;
    }

    ret = libusb_kernel_driver_active(usb_adapter_handle, 0);
    if (ret == 1) {
        ret = libusb_detach_kernel_driver(usb_adapter_handle, 0);
        if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED)
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = %d", ret);
    }

    if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED) {
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    ret = libusb_claim_interface(usb_adapter_handle, 0);
    if (ret) {
        LOG_ERROR(Input, "libusb_claim_interface failed with error = %d", ret);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    return true;
}

static void GetGCEndpoint(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    libusb_get_config_descriptor(device, 0, &config);
    for (u8 ic = 0; ic < config->bNumInterfaces; ic++) {
        const libusb_interface* interfaceContainer = &config->interface[ic];
        for (int i = 0; i < interfaceContainer->num_altsetting; i++) {
            const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
            for (u8 e = 0; e < interface->bNumEndpoints; e++) {
                const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN)
                    input_endpoint = endpoint->bEndpointAddress;
            }
        }
    }

    adapter_thread_running = true;
    current_status = ADAPTER_DETECTED;

    adapter_input_thread = std::thread(Read); // Read input
}

void Shutdown() {
    StopScanThread();
    Reset();

    current_status = NO_ADAPTER_DETECTED;
}

static void Reset() {
    std::unique_lock<std::mutex> lock(initialization_mutex, std::defer_lock);
    if (!lock.try_lock())
        return;
    if (current_status != ADAPTER_DETECTED)
        return;

    if (adapter_thread_running)
        adapter_input_thread.join();

    for (int i = 0; i < 4; i++)
        adapter_controllers_status[i] = ControllerTypes::CONTROLLER_NONE;

    current_status = NO_ADAPTER_DETECTED;

    if (usb_adapter_handle) {
        libusb_release_interface(usb_adapter_handle, 0);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
    }
}

bool DeviceConnected(int port) {
    return adapter_controllers_status[port] != ControllerTypes::CONTROLLER_NONE;
}

void ResetDeviceType(int port) {
    adapter_controllers_status[port] = ControllerTypes::CONTROLLER_NONE;
}

void BeginConfiguration() {
    configuring = true;
}

void EndConfiguration() {
    configuring = false;
}

} // end of namespace GCAdapter
