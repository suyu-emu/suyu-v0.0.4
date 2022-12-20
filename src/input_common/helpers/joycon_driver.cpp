// SPDX-FileCopyrightText: Copyright 2022 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "common/logging/log.h"
#include "common/swap.h"
#include "common/thread.h"
#include "input_common/helpers/joycon_driver.h"

namespace InputCommon::Joycon {
JoyconDriver::JoyconDriver(std::size_t port_) : port{port_} {
    hidapi_handle = std::make_shared<JoyconHandle>();
}

JoyconDriver::~JoyconDriver() {
    Stop();
}

void JoyconDriver::Stop() {
    is_connected = false;
    input_thread = {};
}

DriverResult JoyconDriver::RequestDeviceAccess(SDL_hid_device_info* device_info) {
    std::scoped_lock lock{mutex};

    handle_device_type = ControllerType::None;
    GetDeviceType(device_info, handle_device_type);
    if (handle_device_type == ControllerType::None) {
        return DriverResult::UnsupportedControllerType;
    }

    hidapi_handle->handle =
        SDL_hid_open(device_info->vendor_id, device_info->product_id, device_info->serial_number);
    std::memcpy(&handle_serial_number, device_info->serial_number, 15);
    if (!hidapi_handle->handle) {
        LOG_ERROR(Input, "Yuzu can't gain access to this device: ID {:04X}:{:04X}.",
                  device_info->vendor_id, device_info->product_id);
        return DriverResult::HandleInUse;
    }
    SDL_hid_set_nonblocking(hidapi_handle->handle, 1);
    return DriverResult::Success;
}

DriverResult JoyconDriver::InitializeDevice() {
    if (!hidapi_handle->handle) {
        return DriverResult::InvalidHandle;
    }
    std::scoped_lock lock{mutex};
    disable_input_thread = true;

    // Reset Counters
    error_counter = 0;
    hidapi_handle->packet_counter = 0;

    // Set HW default configuration
    vibration_enabled = true;
    motion_enabled = true;
    hidbus_enabled = false;
    nfc_enabled = false;
    passive_enabled = false;
    gyro_sensitivity = Joycon::GyroSensitivity::DPS2000;
    gyro_performance = Joycon::GyroPerformance::HZ833;
    accelerometer_sensitivity = Joycon::AccelerometerSensitivity::G8;
    accelerometer_performance = Joycon::AccelerometerPerformance::HZ100;

    // Initialize HW Protocols
    generic_protocol = std::make_unique<GenericProtocol>(hidapi_handle);

    // Get fixed joycon info
    generic_protocol->GetVersionNumber(version);
    generic_protocol->GetColor(color);
    if (handle_device_type == ControllerType::Pro) {
        // Some 3rd party controllers aren't pro controllers
        generic_protocol->GetControllerType(device_type);
    } else {
        device_type = handle_device_type;
    }
    generic_protocol->GetSerialNumber(serial_number);
    supported_features = GetSupportedFeatures();

    // Get Calibration data

    // Set led status
    generic_protocol->SetLedBlinkPattern(static_cast<u8>(1 + port));

    // Apply HW configuration
    SetPollingMode();

    // Start pooling for data
    is_connected = true;
    if (!input_thread_running) {
        input_thread =
            std::jthread([this](std::stop_token stop_token) { InputThread(stop_token); });
    }

    disable_input_thread = false;
    return DriverResult::Success;
}

void JoyconDriver::InputThread(std::stop_token stop_token) {
    LOG_INFO(Input, "JC Adapter input thread started");
    Common::SetCurrentThreadName("JoyconInput");
    input_thread_running = true;

    // Max update rate is 5ms, ensure we are always able to read a bit faster
    constexpr int ThreadDelay = 2;
    std::vector<u8> buffer(MaxBufferSize);

    while (!stop_token.stop_requested()) {
        int status = 0;

        if (!IsInputThreadValid()) {
            input_thread.request_stop();
            continue;
        }

        // By disabling the input thread we can ensure custom commands will succeed as no package is
        // skipped
        if (!disable_input_thread) {
            status = SDL_hid_read_timeout(hidapi_handle->handle, buffer.data(), buffer.size(),
                                          ThreadDelay);
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(ThreadDelay));
        }

        if (IsPayloadCorrect(status, buffer)) {
            OnNewData(buffer);
        }

        std::this_thread::yield();
    }

    is_connected = false;
    input_thread_running = false;
    LOG_INFO(Input, "JC Adapter input thread stopped");
}

void JoyconDriver::OnNewData(std::span<u8> buffer) {
    const auto report_mode = static_cast<InputReport>(buffer[0]);

    switch (report_mode) {
    case InputReport::STANDARD_FULL_60HZ:
        ReadActiveMode(buffer);
        break;
    case InputReport::NFC_IR_MODE_60HZ:
        ReadNfcIRMode(buffer);
        break;
    case InputReport::SIMPLE_HID_MODE:
        ReadPassiveMode(buffer);
        break;
    case InputReport::SUBCMD_REPLY:
        LOG_DEBUG(Input, "Unhandled command reply");
        break;
    default:
        LOG_ERROR(Input, "Report mode not Implemented {}", report_mode);
        break;
    }
}

void JoyconDriver::SetPollingMode() {
    disable_input_thread = true;

    if (motion_enabled && supported_features.motion) {
        generic_protocol->EnableImu(true);
        generic_protocol->SetImuConfig(gyro_sensitivity, gyro_performance,
                                       accelerometer_sensitivity, accelerometer_performance);
    } else {
        generic_protocol->EnableImu(false);
    }

    if (passive_enabled && supported_features.passive) {
        const auto result = generic_protocol->EnablePassiveMode();
        if (result == DriverResult::Success) {
            disable_input_thread = false;
            return;
        }
        LOG_ERROR(Input, "Error enabling passive mode");
    }

    // Default Mode
    const auto result = generic_protocol->EnableActiveMode();
    if (result != DriverResult::Success) {
        LOG_ERROR(Input, "Error enabling active mode");
    }

    disable_input_thread = false;
}

JoyconDriver::SupportedFeatures JoyconDriver::GetSupportedFeatures() {
    SupportedFeatures features{
        .passive = true,
        .motion = true,
        .vibration = true,
    };

    if (device_type == ControllerType::Right) {
        features.nfc = true;
        features.irs = true;
        features.hidbus = true;
    }

    if (device_type == ControllerType::Pro) {
        features.nfc = true;
    }
    return features;
}

void JoyconDriver::ReadActiveMode(std::span<u8> buffer) {
    InputReportActive data{};
    memcpy(&data, buffer.data(), sizeof(InputReportActive));

    // Packages can be a litte bit inconsistent. Average the delta time to provide a smoother motion
    // experience
    const auto now = std::chrono::steady_clock::now();
    const auto new_delta_time =
        std::chrono::duration_cast<std::chrono::microseconds>(now - last_update).count();
    delta_time = static_cast<u64>((delta_time * 0.8f) + (new_delta_time * 0.2));
    last_update = now;

    switch (device_type) {
    case Joycon::ControllerType::Left:
        break;
    case Joycon::ControllerType::Right:
        break;
    case Joycon::ControllerType::Pro:
        break;
    case Joycon::ControllerType::Grip:
    case Joycon::ControllerType::Dual:
    case Joycon::ControllerType::None:
        break;
    }

    on_battery_data(data.battery_status);
    on_color_data(color);
}

void JoyconDriver::ReadPassiveMode(std::span<u8> buffer) {
    InputReportPassive data{};
    memcpy(&data, buffer.data(), sizeof(InputReportPassive));

    switch (device_type) {
    case Joycon::ControllerType::Left:
        break;
    case Joycon::ControllerType::Right:
        break;
    case Joycon::ControllerType::Pro:
        break;
    case Joycon::ControllerType::Grip:
    case Joycon::ControllerType::Dual:
    case Joycon::ControllerType::None:
        break;
    }
}

void JoyconDriver::ReadNfcIRMode(std::span<u8> buffer) {
    // This mode is compatible with the active mode
    ReadActiveMode(buffer);

    if (!nfc_enabled) {
        return;
    }
}

bool JoyconDriver::IsInputThreadValid() const {
    if (!is_connected) {
        return false;
    }
    if (hidapi_handle->handle == nullptr) {
        return false;
    }
    // Controller is not responding. Terminate connection
    if (error_counter > MaxErrorCount) {
        return false;
    }
    return true;
}

bool JoyconDriver::IsPayloadCorrect(int status, std::span<const u8> buffer) {
    if (status <= -1) {
        error_counter++;
        return false;
    }
    // There's no new data
    if (status == 0) {
        return false;
    }
    // No reply ever starts with zero
    if (buffer[0] == 0x00) {
        error_counter++;
        return false;
    }
    error_counter = 0;
    return true;
}

DriverResult JoyconDriver::SetVibration(const VibrationValue& vibration) {
    std::scoped_lock lock{mutex};
    if (disable_input_thread) {
        return DriverResult::HandleInUse;
    }
    return DriverResult::NotSupported;
}

DriverResult JoyconDriver::SetLedConfig(u8 led_pattern) {
    std::scoped_lock lock{mutex};
    if (disable_input_thread) {
        return DriverResult::HandleInUse;
    }
    return generic_protocol->SetLedPattern(led_pattern);
}

DriverResult JoyconDriver::SetPasiveMode() {
    std::scoped_lock lock{mutex};
    motion_enabled = false;
    hidbus_enabled = false;
    nfc_enabled = false;
    passive_enabled = true;
    SetPollingMode();
    return DriverResult::Success;
}

DriverResult JoyconDriver::SetActiveMode() {
    std::scoped_lock lock{mutex};
    motion_enabled = true;
    hidbus_enabled = false;
    nfc_enabled = false;
    passive_enabled = false;
    SetPollingMode();
    return DriverResult::Success;
}

DriverResult JoyconDriver::SetNfcMode() {
    std::scoped_lock lock{mutex};
    motion_enabled = false;
    hidbus_enabled = false;
    nfc_enabled = true;
    passive_enabled = false;
    SetPollingMode();
    return DriverResult::Success;
}

DriverResult JoyconDriver::SetRingConMode() {
    std::scoped_lock lock{mutex};
    motion_enabled = true;
    hidbus_enabled = true;
    nfc_enabled = false;
    passive_enabled = false;
    SetPollingMode();
    return DriverResult::Success;
}

bool JoyconDriver::IsConnected() const {
    std::scoped_lock lock{mutex};
    return is_connected;
}

bool JoyconDriver::IsVibrationEnabled() const {
    std::scoped_lock lock{mutex};
    return vibration_enabled;
}

FirmwareVersion JoyconDriver::GetDeviceVersion() const {
    std::scoped_lock lock{mutex};
    return version;
}

Color JoyconDriver::GetDeviceColor() const {
    std::scoped_lock lock{mutex};
    return color;
}

std::size_t JoyconDriver::GetDevicePort() const {
    std::scoped_lock lock{mutex};
    return port;
}

ControllerType JoyconDriver::GetDeviceType() const {
    std::scoped_lock lock{mutex};
    return device_type;
}

ControllerType JoyconDriver::GetHandleDeviceType() const {
    std::scoped_lock lock{mutex};
    return handle_device_type;
}

SerialNumber JoyconDriver::GetSerialNumber() const {
    std::scoped_lock lock{mutex};
    return serial_number;
}

SerialNumber JoyconDriver::GetHandleSerialNumber() const {
    std::scoped_lock lock{mutex};
    return handle_serial_number;
}

Joycon::DriverResult JoyconDriver::GetDeviceType(SDL_hid_device_info* device_info,
                                                 ControllerType& controller_type) {
    std::array<std::pair<u32, Joycon::ControllerType>, 4> supported_devices{
        std::pair<u32, Joycon::ControllerType>{0x2006, Joycon::ControllerType::Left},
        {0x2007, Joycon::ControllerType::Right},
        {0x2009, Joycon::ControllerType::Pro},
        {0x200E, Joycon::ControllerType::Grip},
    };
    constexpr u16 nintendo_vendor_id = 0x057e;

    controller_type = Joycon::ControllerType::None;
    if (device_info->vendor_id != nintendo_vendor_id) {
        return Joycon::DriverResult::UnsupportedControllerType;
    }

    for (const auto& [product_id, type] : supported_devices) {
        if (device_info->product_id == static_cast<u16>(product_id)) {
            controller_type = type;
            return Joycon::DriverResult::Success;
        }
    }
    return Joycon::DriverResult::UnsupportedControllerType;
}

Joycon::DriverResult JoyconDriver::GetSerialNumber(SDL_hid_device_info* device_info,
                                                   Joycon::SerialNumber& serial_number) {
    if (device_info->serial_number == nullptr) {
        return Joycon::DriverResult::Unknown;
    }
    std::memcpy(&serial_number, device_info->serial_number, 15);
    return Joycon::DriverResult::Success;
}

} // namespace InputCommon::Joycon
