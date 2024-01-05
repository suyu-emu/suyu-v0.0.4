// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core_timing.h"
#include "hid_core/resources/applet_resource.h"
#include "hid_core/resources/shared_memory_format.h"
#include "hid_core/resources/system_buttons/sleep_button.h"

namespace Service::HID {

SleepButton::SleepButton(Core::HID::HIDCore& hid_core_) : ControllerBase{hid_core_} {}

SleepButton::~SleepButton() = default;

void SleepButton::OnInit() {}

void SleepButton::OnRelease() {}

void SleepButton::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    if (!smart_update) {
        return;
    }

    std::scoped_lock shared_lock{*shared_mutex};
    const u64 aruid = applet_resource->GetActiveAruid();
    auto* data = applet_resource->GetAruidData(aruid);

    if (data == nullptr || !data->flag.is_assigned) {
        return;
    }

    auto& header = data->shared_memory_format->capture_button.header;
    header.timestamp = core_timing.GetGlobalTimeNs().count();
    header.total_entry_count = 17;
    header.entry_count = 0;
    header.last_entry_index = 0;
}

} // namespace Service::HID
