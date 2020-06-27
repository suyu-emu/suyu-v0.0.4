// Copyright 2014 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <chrono>
#include <thread>
#include "common/logging/log.h"
#include "input_common/gcadapter/gc_adapter.h"

namespace GCAdapter {

Adapter::Adapter() {
    if (usb_adapter_handle != nullptr) {
        return;
    }
    LOG_INFO(Input, "GC Adapter Initialization started");

    current_status = NO_ADAPTER_DETECTED;
    libusb_init(&libusb_ctx);

    StartScanThread();
}

GCPadStatus Adapter::GetPadStatus(int port, const std::array<u8, 37>& adapter_payload) {
    GCPadStatus pad = {};
    bool get_origin = false;

    ControllerTypes type = ControllerTypes(adapter_payload[1 + (9 * port)] >> 4);
    if (type != ControllerTypes::None) {
        get_origin = true;
    }

    adapter_controllers_status[port] = type;

    constexpr std::array<PadButton, 8> b1_buttons{
        PadButton::PAD_BUTTON_A,    PadButton::PAD_BUTTON_B,    PadButton::PAD_BUTTON_X,
        PadButton::PAD_BUTTON_Y,    PadButton::PAD_BUTTON_LEFT, PadButton::PAD_BUTTON_RIGHT,
        PadButton::PAD_BUTTON_DOWN, PadButton::PAD_BUTTON_UP};

    constexpr std::array<PadButton, 4> b2_buttons{
        PadButton::PAD_BUTTON_START, PadButton::PAD_TRIGGER_Z, PadButton::PAD_TRIGGER_R,
        PadButton::PAD_TRIGGER_L};

    if (adapter_controllers_status[port] != ControllerTypes::None) {
        const u8 b1 = adapter_payload[1 + (9 * port) + 1];
        const u8 b2 = adapter_payload[1 + (9 * port) + 2];

        for (int i = 0; i < b1_buttons.size(); i++) {
            if (b1 & (1 << i)) {
                pad.button |= static_cast<u16>(b1_buttons[i]);
            }
        }

        for (int j = 0; j < b2_buttons.size(); j++) {
            if (b2 & (1 << j)) {
                pad.button |= static_cast<u16>(b2_buttons[j]);
            }
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

void Adapter::PadToState(const GCPadStatus& pad, GCState& state) {
    for (const auto& button : PadButtonArray) {
        u16 button_value = static_cast<u16>(button);
        state.buttons.insert_or_assign(button_value, pad.button & button_value);
    }

    state.axes.insert_or_assign(static_cast<u8>(PadAxes::StickX), pad.stick_x);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::StickY), pad.stick_y);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::SubstickX), pad.substick_x);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::SubstickY), pad.substick_y);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::TriggerLeft), pad.trigger_left);
    state.axes.insert_or_assign(static_cast<u8>(PadAxes::TriggerRight), pad.trigger_right);
}

void Adapter::Read() {
    LOG_INFO(Input, "GC Adapter Read() thread started");

    int payload_size_in, payload_size_copy;
    std::array<u8, 37> adapter_payload;
    std::array<u8, 37> adapter_payload_copy;
    std::array<GCPadStatus, 4> pads;

    while (adapter_thread_running) {
        libusb_interrupt_transfer(usb_adapter_handle, input_endpoint, adapter_payload.data(),
                                  sizeof(adapter_payload), &payload_size_in, 16);
        payload_size_copy = 0;
        {
            std::lock_guard<std::mutex> lk(s_mutex);
            std::copy(std::begin(adapter_payload), std::end(adapter_payload),
                      std::begin(adapter_payload_copy));
            payload_size_copy = payload_size_in;
        }

        if (payload_size_copy != sizeof(adapter_payload_copy) ||
            adapter_payload_copy[0] != LIBUSB_DT_HID) {
            LOG_ERROR(Input, "error reading payload (size: %d, type: %02x)", payload_size_copy,
                      adapter_payload_copy[0]);
            adapter_thread_running = false; // error reading from adapter, stop reading.
        } else {
            for (int port = 0; port < pads.size(); port++) {
                pads[port] = GetPadStatus(port, adapter_payload_copy);
            }
        }
        for (int port = 0; port < pads.size(); port++) {
            if (DeviceConnected(port) && configuring) {
                if (pads[port].button != PAD_GET_ORIGIN) {
                    pad_queue[port].Push(pads[port]);
                }

                // Accounting for a threshold here because of some controller variance
                if (pads[port].stick_x > pads[port].MAIN_STICK_CENTER_X + pads[port].THRESHOLD ||
                    pads[port].stick_x < pads[port].MAIN_STICK_CENTER_X - pads[port].THRESHOLD) {
                    pads[port].axis = GCAdapter::PadAxes::StickX;
                    pads[port].axis_value = pads[port].stick_x;
                    pad_queue[port].Push(pads[port]);
                }
                if (pads[port].stick_y > pads[port].MAIN_STICK_CENTER_Y + pads[port].THRESHOLD ||
                    pads[port].stick_y < pads[port].MAIN_STICK_CENTER_Y - pads[port].THRESHOLD) {
                    pads[port].axis = GCAdapter::PadAxes::StickY;
                    pads[port].axis_value = pads[port].stick_y;
                    pad_queue[port].Push(pads[port]);
                }
                if (pads[port].substick_x > pads[port].C_STICK_CENTER_X + pads[port].THRESHOLD ||
                    pads[port].substick_x < pads[port].C_STICK_CENTER_X - pads[port].THRESHOLD) {
                    pads[port].axis = GCAdapter::PadAxes::SubstickX;
                    pads[port].axis_value = pads[port].substick_x;
                    pad_queue[port].Push(pads[port]);
                }
                if (pads[port].substick_y > pads[port].C_STICK_CENTER_Y + pads[port].THRESHOLD ||
                    pads[port].substick_y < pads[port].C_STICK_CENTER_Y - pads[port].THRESHOLD) {
                    pads[port].axis = GCAdapter::PadAxes::SubstickY;
                    pads[port].axis_value = pads[port].substick_y;
                    pad_queue[port].Push(pads[port]);
                }
            }
            PadToState(pads[port], state[port]);
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
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
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
    detect_thread_running = false;
    detect_thread.join();
}

void Adapter::Setup() {
    // Reset the error status in case the adapter gets unplugged
    if (current_status < 0) {
        current_status = NO_ADAPTER_DETECTED;
    }

    adapter_controllers_status.fill(ControllerTypes::None);

    libusb_device** devs; // pointer to list of connected usb devices

    const int cnt = libusb_get_device_list(libusb_ctx, &devs); // get the list of devices

    for (int i = 0; i < cnt; i++) {
        if (CheckDeviceAccess(devs[i])) {
            // GC Adapter found and accessible, registering it
            GetGCEndpoint(devs[i]);
            break;
        }
    }
}

bool Adapter::CheckDeviceAccess(libusb_device* device) {
    libusb_device_descriptor desc;
    const int get_descriptor_error = libusb_get_device_descriptor(device, &desc);
    if (get_descriptor_error) {
        // could not acquire the descriptor, no point in trying to use it.
        LOG_ERROR(Input, "libusb_get_device_descriptor failed with error: %d",
                  get_descriptor_error);
        return false;
    }

    if (desc.idVendor != 0x057e || desc.idProduct != 0x0337) {
        // This isn't the device we are looking for.
        return false;
    }
    const int open_error = libusb_open(device, &usb_adapter_handle);

    if (open_error == LIBUSB_ERROR_ACCESS) {
        LOG_ERROR(Input, "Yuzu can not gain access to this device: ID %04X:%04X.", desc.idVendor,
                  desc.idProduct);
        return false;
    }
    if (open_error) {
        LOG_ERROR(Input, "libusb_open failed to open device with error = %d", open_error);
        return false;
    }

    int kernel_driver_error = libusb_kernel_driver_active(usb_adapter_handle, 0);
    if (kernel_driver_error == 1) {
        kernel_driver_error = libusb_detach_kernel_driver(usb_adapter_handle, 0);
        if (kernel_driver_error != 0 && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
            LOG_ERROR(Input, "libusb_detach_kernel_driver failed with error = %d",
                      kernel_driver_error);
        }
    }

    if (kernel_driver_error && kernel_driver_error != LIBUSB_ERROR_NOT_SUPPORTED) {
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
        return false;
    }

    const int interface_claim_error = libusb_claim_interface(usb_adapter_handle, 0);
    if (interface_claim_error) {
        LOG_ERROR(Input, "libusb_claim_interface failed with error = %d", interface_claim_error);
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
        adapter_thread_running = false;
    }
    adapter_input_thread.join();

    adapter_controllers_status.fill(ControllerTypes::None);
    current_status = NO_ADAPTER_DETECTED;

    if (usb_adapter_handle) {
        libusb_release_interface(usb_adapter_handle, 1);
        libusb_close(usb_adapter_handle);
        usb_adapter_handle = nullptr;
    }

    if (libusb_ctx) {
        libusb_exit(libusb_ctx);
    }
}

bool Adapter::DeviceConnected(int port) {
    return adapter_controllers_status[port] != ControllerTypes::None;
}

void Adapter::ResetDeviceType(int port) {
    adapter_controllers_status[port] = ControllerTypes::None;
}

void Adapter::BeginConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = true;
}

void Adapter::EndConfiguration() {
    for (auto& pq : pad_queue) {
        pq.Clear();
    }
    configuring = false;
}

std::array<Common::SPSCQueue<GCPadStatus>, 4>& Adapter::GetPadQueue() {
    return pad_queue;
}

const std::array<Common::SPSCQueue<GCPadStatus>, 4>& Adapter::GetPadQueue() const {
    return pad_queue;
}

std::array<GCState, 4>& Adapter::GetPadState() {
    return state;
}

const std::array<GCState, 4>& Adapter::GetPadState() const {
    return state;
}

} // namespace GCAdapter
