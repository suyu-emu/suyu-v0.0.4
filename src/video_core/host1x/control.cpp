// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/assert.h"
#include "video_core/host1x/control.h"
#include "video_core/host1x/host1x.h"

namespace Tegra::Host1x {

void Control::ProcessMethod(Host1x& host1x, Method method, u32 argument) {
    switch (method) {
    case Method::LoadSyncptPayload32:
        syncpoint_value = argument;
        break;
    case Method::WaitSyncpt:
    case Method::WaitSyncpt32:
        Execute(host1x, argument);
        break;
    default:
        UNIMPLEMENTED_MSG("Control method {:#X}", u32(method));
        break;
    }
}

void Control::Execute(Host1x& host1x, u32 data) {
    LOG_TRACE(Service_NVDRV, "Control wait syncpt {} value {}", data, syncpoint_value);
    host1x.GetSyncpointManager().WaitHost(data, syncpoint_value);
}

} // namespace Tegra::Host1x
