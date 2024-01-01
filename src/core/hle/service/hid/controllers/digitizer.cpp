// SPDX-FileCopyrightText: Copyright 2018 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/core_timing.h"
#include "core/hle/service/hid/controllers/applet_resource.h"
#include "core/hle/service/hid/controllers/digitizer.h"
#include "core/hle/service/hid/controllers/types/shared_memory_format.h"

namespace Service::HID {

Digitizer::Digitizer(Core::HID::HIDCore& hid_core_) : ControllerBase{hid_core_} {}

Digitizer::~Digitizer() = default;

void Digitizer::OnInit() {}

void Digitizer::OnRelease() {}

void Digitizer::OnUpdate(const Core::Timing::CoreTiming& core_timing) {
    if (!smart_update) {
        return;
    }

    const u64 aruid = applet_resource->GetActiveAruid();
    auto* data = applet_resource->GetAruidData(aruid);

    if (data == nullptr) {
        return;
    }

    auto& header = data->shared_memory_format->digitizer.header;
    header.timestamp = core_timing.GetGlobalTimeNs().count();
    header.total_entry_count = 17;
    header.entry_count = 0;
    header.last_entry_index = 0;
}

} // namespace Service::HID
