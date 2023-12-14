// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "common/common_types.h"
#include "core/hle/service/hid/controllers/controller_base.h"
#include "core/hle/service/hid/controllers/types/debug_pad_types.h"

namespace Core::HID {
class HIDCore;
}

namespace Core::Timing {
class CoreTiming;
}

namespace Service::HID {
struct DebugPadSharedMemoryFormat;

class DebugPad final : public ControllerBase {
public:
    explicit DebugPad(Core::HID::HIDCore& hid_core_,
                      DebugPadSharedMemoryFormat& debug_pad_shared_memory);
    ~DebugPad() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

private:
    DebugPadState next_state{};
    DebugPadSharedMemoryFormat& shared_memory;
    Core::HID::EmulatedController* controller = nullptr;
};
} // namespace Service::HID
