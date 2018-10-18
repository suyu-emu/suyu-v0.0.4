// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/common_types.h"
#include "core/core_timing.h"
#include "core/hle/service/hid/controllers/debug_pad.h"

namespace Service::HID {

Controller_DebugPad::Controller_DebugPad() = default;
Controller_DebugPad::~Controller_DebugPad() = default;

void Controller_DebugPad::OnInit() {}

void Controller_DebugPad::OnRelease() {}

void Controller_DebugPad::OnUpdate(u8* data, std::size_t size) {
    shared_memory.header.timestamp = CoreTiming::GetTicks();
    shared_memory.header.total_entry_count = 17;

    if (!IsControllerActivated()) {
        shared_memory.header.entry_count = 0;
        shared_memory.header.last_entry_index = 0;
        return;
    }
    shared_memory.header.entry_count = 16;

    const auto& last_entry = shared_memory.pad_states[shared_memory.header.last_entry_index];
    shared_memory.header.last_entry_index = (shared_memory.header.last_entry_index + 1) % 17;
    auto& cur_entry = shared_memory.pad_states[shared_memory.header.last_entry_index];

    cur_entry.sampling_number = last_entry.sampling_number + 1;
    cur_entry.sampling_number2 = cur_entry.sampling_number;
    // TODO(ogniK): Update debug pad states
    cur_entry.attribute.connected.Assign(1);
    auto& pad = cur_entry.pad_state;

    pad.a.Assign(0);
    pad.b.Assign(0);
    pad.x.Assign(0);
    pad.y.Assign(0);
    pad.l.Assign(0);
    pad.r.Assign(0);
    pad.zl.Assign(0);
    pad.zr.Assign(0);
    pad.plus.Assign(0);
    pad.minus.Assign(0);
    pad.d_left.Assign(0);
    pad.d_up.Assign(0);
    pad.d_right.Assign(0);
    pad.d_down.Assign(0);

    cur_entry.l_stick.x = 0;
    cur_entry.l_stick.y = 0;

    cur_entry.r_stick.x = 0;
    cur_entry.r_stick.y = 0;

    std::memcpy(data, &shared_memory, sizeof(SharedMemory));
}

void Controller_DebugPad::OnLoadInputDevices() {}
} // namespace Service::HID
