// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core {
class System;
}

namespace Service::Nvnflinger {
class Nvnflinger;
}

namespace Service::AM {

void LoopProcess(Nvnflinger::Nvnflinger& nvnflinger, Core::System& system);

} // namespace Service::AM
