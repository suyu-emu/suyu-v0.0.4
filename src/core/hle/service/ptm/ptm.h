// SPDX-FileCopyrightText: Copyright 2022 suyu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace Core {
class System;
}

namespace Service::PTM {

void LoopProcess(Core::System& system);

} // namespace Service::PTM
