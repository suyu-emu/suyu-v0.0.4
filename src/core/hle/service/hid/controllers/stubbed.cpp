// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core_timing.h"
#include "core/hle/service/hid/controllers/shared_memory_format.h"
#include "core/hle/service/hid/controllers/stubbed.h"

namespace Service::HID {

Controller_Stubbed::Controller_Stubbed(Core::HID::HIDCore& hid_core_,
                                       CommonHeader& ring_lifo_header)
    : ControllerBase{hid_core_}, header{ring_lifo_header} {}

Controller_Stubbed::~Controller_Stubbed() = default;

void Controller_Stubbed::OnInit() {}

void Controller_Stubbed::OnRelease() {}

void Controller_Stubbed::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    if (!smart_update) {
        return;
    }

    header.timestamp = core_timing.GetGlobalTimeNs().count();
    header.total_entry_count = 17;
    header.entry_count = 0;
    header.last_entry_index = 0;
}

} // namespace Service::HID
