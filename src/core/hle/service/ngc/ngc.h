// SPDX-FileCopyrightText: Copyright 2021 suyu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

namespace Core {
class System;
}

namespace Service::NGC {

void LoopProcess(Core::System& system);

} // namespace Service::NGC
