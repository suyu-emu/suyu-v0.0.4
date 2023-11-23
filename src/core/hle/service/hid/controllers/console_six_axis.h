// SPDX-FileCopyrightText: Copyright 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/vector_math.h"
#include "core/hle/service/hid/controllers/controller_base.h"

namespace Core::HID {
class EmulatedConsole;
} // namespace Core::HID

namespace Service::HID {
class ConsoleSixAxis final : public ControllerBase {
public:
    explicit ConsoleSixAxis(Core::HID::HIDCore& hid_core_, u8* raw_shared_memory_);
    ~ConsoleSixAxis() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

private:
    // This is nn::hid::detail::ConsoleSixAxisSensorSharedMemoryFormat
    struct ConsoleSharedMemory {
        u64 sampling_number{};
        bool is_seven_six_axis_sensor_at_rest{};
        INSERT_PADDING_BYTES(3); // padding
        f32 verticalization_error{};
        Common::Vec3f gyro_bias{};
        INSERT_PADDING_BYTES(4); // padding
    };
    static_assert(sizeof(ConsoleSharedMemory) == 0x20, "ConsoleSharedMemory is an invalid size");

    ConsoleSharedMemory* shared_memory = nullptr;
    Core::HID::EmulatedConsole* console = nullptr;
};
} // namespace Service::HID
