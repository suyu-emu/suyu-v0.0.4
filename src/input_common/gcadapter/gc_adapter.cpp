// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "input_common/gcadapter/gc_adapter.h"

namespace GCAdapter {
Adapter* Adapter::adapter_instance{nullptr};

Adapter::Adapter() {
    if (usb_adapter_handle != nullptr) {
        return;
    }
    LOG_INFO(Input, "GC Adapter Initialization started");

    current_status = NO_ADAPTER_DETECTED;
    libusb_init(&libusb_ctx);

    StartScanThread();
}

Adapter* Adapter::GetInstance() {
    if (!adapter_instance) {
        adapter_instance = new Adapter;
    }
    return adapter_instance;
}

GCPadStatus Adapter::CheckStatus(int port, u8 adapter_payload[37]) {
    GCPadStatus pad = {};
    bool get_origin = false;

    ControllerTypes type = ControllerTypes(adapter_payload[1 + (9 * port)] >> 4);
    if (type != ControllerTypes::None)
        get_origin = true;

    adapter_controllers_status[port] = type;

    if (adapter_controllers_status[port] != ControllerTypes::None) {
        u8 b1 = adapter_payload[1 + (9 * port) + 1];
        u8 b2 = adapter_payload[1 + (9 * port) + 2];

        if (b1 & (1 << 0)) {
            pad.button |= PAD_BUTTON_A;
        }
        if (b1 & (1 << 1)) {
            pad.button |= PAD_BUTTON_B;
        }
        if (b1 & (1 << 2)) {
            pad.button |= PAD_BUTTON_X;
        }
        if (b1 & (1 << 3)) {
            pad.button |= PAD_BUTTON_Y;
        }

        if (b1 & (1 << 4)) {
            pad.button |= PAD_BUTTON_LEFT;
        }
        if (b1 & (1 << 5)) {
            pad.button |= PAD_BUTTON_RIGHT;
        }
        if (b1 & (1 << 6)) {
            pad.button |= PAD_BUTTON_DOWN;
        }
        if (b1 & (1 << 7)) {
            pad.button |= PAD_BUTTON_UP;
        }

        if (b2 & (1 << 0)) {
            pad.button |= PAD_BUTTON_START;
        }
        if (b2 & (1 << 1)) {
            pad.button |= PAD_TRIGGER_Z;
        }
        if (b2 & (1 << 2)) {
            pad.button |= PAD_TRIGGER_R;
        }
        if (b2 & (1 << 3)) {
            pad.button |= PAD_TRIGGER_L;
        }

        if (get_origin) {
            pad.button |= PAD_GET_ORIGIN;
        }

        pad.stick_x = adapter_payload[1 + (9 * port) + 3];
        pad.stick_y = adapter_payload[1 + (9 * port) + 4];
        pad.substick_x = adapter_payload[1 + (9 * port) + 5];
        pad.substick_y = adapter_payload[1 + (9 * port) + 6];
        pad.trigger_left = adapter_payload[1 + (9 * port) + 7];
        pad.trigger_right = adapter_payload[1 + (9 * port) + 8];
    }
    return pad;
}

void Adapter::PadToState(GCPadStatus pad, GCState& state) {
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
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::StickX), pad.stick_x);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::StickY), pad.stick_y);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::SubstickX), pad.substick_x);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::SubstickY), pad.substick_y);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::TriggerLeft), pad.trigger_left);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::TriggerRight), pad.trigger_right);
}

void Adapter::Read() {
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
            for (int port = 0; port < 4; port++) {
                pad[port] = CheckStatus(port, controller_payload_copy);
            }
        }
        for (int port = 0; port < 4; port++) {
            if (DeviceConnected(port) && configuring) {
                if (pad[port].button != PAD_GET_ORIGIN) {
                    pad_queue[port].Push(pad[port]);
                }

                // Accounting for a threshold here because of some controller variance
                if (pad[port].stick_x >
                        pad_constants.MAIN_STICK_CENTER_X + pad_constants.THRESHOLD ||
                    pad[port].stick_x <
                        pad_constants.MAIN_STICK_CENTER_X - pad_constants.THRESHOLD) {
                    pad[port].axis = GCAdapter::PadAxes::StickX;
                    pad[port].axis_value = pad[port].stick_x;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].stick_y >
                        pad_constants.MAIN_STICK_CENTER_Y + pad_constants.THRESHOLD ||
                    pad[port].stick_y <
                        pad_constants.MAIN_STICK_CENTER_Y - pad_constants.THRESHOLD) {
                    pad[port].axis = GCAdapter::PadAxes::StickY;
                    pad[port].axis_value = pad[port].stick_y;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].substick_x >
                        pad_constants.C_STICK_CENTER_X + pad_constants.THRESHOLD ||
                    pad[port].substick_x <
                        pad_constants.C_STICK_CENTER_X - pad_constants.THRESHOLD) {
                    pad[port].axis = GCAdapter::PadAxes::SubstickX;
                    pad[port].axis_value = pad[port].substick_x;
                    pad_queue[port].Push(pad[port]);
                }
                if (pad[port].substick_y >
                        pad_constants.C_STICK_CENTER_Y + pad_constants.THRESHOLD ||
                    pad[port].substick_y <
                        pad_constants.C_STICK_CENTER_Y - pad_constants.THRESHOLD) {
                    pad[port].axis = GCAdapter::PadAxes::SubstickY;
                    pad[port].axis_value = pad[port].substick_y;
                    pad_queue[port].Push(pad[port]);
                }
            }
            PadToState(pad[port], state[port]);
        }
        std::this_thread::yield();
    }
}

void Adapter::ScanThreadFunc() {
    LOG_INFO(Input, "GC Adapter scanning thread started");

    while (detect_thread_running) {
        if (usb_adapter_handle == nullptr) {
            std::lock_guard<std::mutex> lk(initialization_mutex);
            Setup();
        }
        Sleep(500);
    }
}

void Adapter::StartScanThread() {
    if (detect_thread_running) {
        return;
    }
    if (!libusb_ctx) {
        return;
    }

    detect_thread_running = true;
    detect_thread = std::thread([=] { ScanThreadFunc(); });
}

void Adapter::StopScanThread() {
    detect_thread.join();
}

void Adapter::Setup() {
    // Reset the error status in case the adapter gets unplugged
    if (current_status < 0) {
        current_status = NO_ADAPTER_DETECTED;
    }

    for (int i = 0; i < 4; i++) {
        adapter_controllers_status[i] = ControllerTypes::None;
    }

    libusb_device** devs; // pointer to list of connected usb devices

    int cnt = libusb_get_device_list(libusb_ctx, &devs); // get the list of devices

    for (int i = 0; i < cnt; i++) {
        if (CheckDeviceAccess(devs[i])) {
            // GC Adapter found, registering it
            GetGCEndpoint(devs[i]);
            break;
        }
    }
}

bool Adapter::CheckDeviceAccess(libusb_device* device) {
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
        LOG_ERROR(Input, "Yuzu can not gain access to this device: ID %04X:%04X.", desc.idVendor,
                  desc.idProduct);
        return false;
    }
    if (ret) {
        LOG_ERROR(Input, "libusb_open failed to open device with error = %d", ret);
        return false;
    }

    ret = libusb_kernel_driver_active(usb_adapter_handle, 0);
    if (ret == 1) {
        ret = libusb_detach_kernel_driver(usb_adapter_handle, 0);
        if (ret != 0 && ret != LIBUSB_ERROR_NOT_SUPPORTED) {
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = %d", ret);
        }
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

void Adapter::GetGCEndpoint(libusb_device* device) {
    libusb_config_descriptor* config = nullptr;
    libusb_get_config_descriptor(device, 0, &config);
    for (u8 ic = 0; ic < config->bNumInterfaces; ic++) {
        const libusb_interface* interfaceContainer = &config->interface[ic];
        for (int i = 0; i < interfaceContainer->num_altsetting; i++) {
            const libusb_interface_descriptor* interface = &interfaceContainer->altsetting[i];
            for (u8 e = 0; e < interface->bNumEndpoints; e++) {
                const libusb_endpoint_descriptor* endpoint = &interface->endpoint[e];
                if (endpoint->bEndpointAddress & LIBUSB_ENDPOINT_IN) {
                    input_endpoint = endpoint->bEndpointAddress;
                }
            }
        }
    }

    adapter_thread_running = true;
    current_status = ADAPTER_DETECTED;
    adapter_input_thread = std::thread([=] { Read(); }); // Read input
}

Adapter::~Adapter() {
    StopScanThread();
    Reset();

    current_status = NO_ADAPTER_DETECTED;
}

void Adapter::Reset() {
    std::unique_lock<std::mutex> lock(initialization_mutex, std::defer_lock);
    if (!lock.try_lock()) {
        return;
    }
    if (current_status != ADAPTER_DETECTED) {
        return;
    }

    if (adapter_thread_running) {
        adapter_input_thread.join();
    }

    for (int i = 0; i < 4; i++) {
        adapter_controllers_status[i] = ControllerTypes::None;
    }

    current_status = NO_ADAPTER_DETECTED;

    if (usb_adapter_handle) {
        libusb_release_interface(usb_adapter_handle, 0);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
    }
}

bool Adapter::DeviceConnected(int port) {
    return adapter_controllers_status[port] != ControllerTypes::None;
}

void Adapter::ResetDeviceType(int port) {
    adapter_controllers_status[port] = ControllerTypes::None;
}

void Adapter::BeginConfiguration() {
    configuring = true;
}

void Adapter::EndConfiguration() {
    configuring = false;
}

std::array<Common::SPSCQueue<GCPadStatus>, 4>& Adapter::GetPadQueue() {
    return pad_queue;
}

std::array<GCState, 4>& Adapter::GetPadState() {
    return state;
}

} // end of namespace GCAdapter
