// Copyright 2020 yuzu Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "video_core/command_classes/host1x.h"
#include "video_core/gpu.h"

Tegra::Host1x::Host1x(GPU& gpu_) : gpu(gpu_) {}

Tegra::Host1x::~Host1x() = default;

void Tegra::Host1x::StateWrite(u32 offset, u32 arguments) {
    u8* const state_offset = reinterpret_cast<u8*>(&state) + offset * sizeof(u32);
    std::memcpy(state_offset, &arguments, sizeof(u32));
}

void Tegra::Host1x::ProcessMethod(Method method, const std::vector<u32>& arguments) {
    StateWrite(static_cast<u32>(method), arguments[0]);
    switch (method) {
    case Method::WaitSyncpt:
        Execute(arguments[0]);
        break;
    case Method::LoadSyncptPayload32:
        syncpoint_value = arguments[0];
        break;
    case Method::WaitSyncpt32:
        Execute(arguments[0]);
        break;
    default:
        UNIMPLEMENTED_MSG("Host1x method 0x{:X}", static_cast<u32>(method));
        break;
    }
}

void Tegra::Host1x::Execute(u32 data) {
    // This method waits on a valid syncpoint.
    // TODO: Implement when proper Async is in place
}
