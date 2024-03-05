// SPDX-FileCopyrightText: Copyright 2023 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core {
class System;
}

namespace Service::RO {

void LoopProcess(Core::System& system);

} // namespace Service::RO
