// Copyright 2018 yuzu emulator team
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/swap.h"
#include "core/core_timing.h"
#include "core/hle/service/hid/controllers/stubbed.h"

namespace Service::HID {
void Controller_Stubbed::OnInit() {}
void Controller_Stubbed::OnRelease() {}
void Controller_Stubbed::OnUpdate(u8* data, size_t size) {
    if (!smart_update) {
        return;
    }

    CommonHeader header{};
    header.timestamp = CoreTiming::GetTicks();
    header.total_entry_count = 17;
    header.entry_count = 0;
    header.last_entry_index = 0;

    std::memcpy(data + common_offset, &header, sizeof(CommonHeader));
}
void Controller_Stubbed::OnLoadInputDevices() {}

void Controller_Stubbed::SetCommonHeaderOffset(size_t off) {
    common_offset = off;
    smart_update = true;
}
}; // namespace Service::HID
