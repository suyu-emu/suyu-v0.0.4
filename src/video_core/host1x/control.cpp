// Copyright 2022 yuzu Emulator Project
// Licensed under GPLv3 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "video_core/gpu.h"
#include "video_core/host1x/control.h"
#include "video_core/host1x/host1x.h"

namespace Tegra::Host1x {

Control::Control(GPU& gpu_) : gpu(gpu_) {}

Control::~Control() = default;

void Control::ProcessMethod(Method method, u32 argument) {
    switch (method) {
    case Method::LoadSyncptPayload32:
        syncpoint_value = argument;
        break;
    case Method::WaitSyncpt:
    case Method::WaitSyncpt32:
        Execute(argument);
        break;
    default:
        UNIMPLEMENTED_MSG("Control method 0x{:X}", static_cast<u32>(method));
        break;
    }
}

void Control::Execute(u32 data) {
    gpu.Host1x().GetSyncpointManager().WaitHost(data, syncpoint_value);
}

} // namespace Tegra::Host1x
