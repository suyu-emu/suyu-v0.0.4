// SPDX-FileCopyrightText: 2021 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/core.h"
#include "video_core/host1x/host1x.h"

namespace Tegra {

namespace Host1x {

Host1x::Host1x(Core::System& system_)
    : system{system_}, syncpoint_manager{}, memory_manager(system.DeviceMemory()) {}

} // namespace Host1x

} // namespace Tegra
