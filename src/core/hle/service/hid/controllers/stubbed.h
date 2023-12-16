// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/hid/controllers/controller_base.h"

namespace Service::HID {
struct CommonHeader;

class Controller_Stubbed final : public ControllerBase {
public:
    explicit Controller_Stubbed(Core::HID::HIDCore& hid_core_, CommonHeader& ring_lifo_header);
    ~Controller_Stubbed() override;

    // Called when the controller is initialized
    void OnInit() override;

    // When the controller is released
    void OnRelease() override;

    // When the controller is requesting an update for the shared memory
    void OnUpdate(const Core::Timing::CoreTiming& core_timing) override;

private:
    CommonHeader& header;
    bool smart_update{};
};
} // namespace Service::HID
