// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/service/system_process_common_functions.h"

namespace Service::AM {

ISystemProcessCommonFunctions::ISystemProcessCommonFunctions(Core::System& system_)
    : ServiceFramework{system_, "ISystemProcessCommonFunctions"} {}

ISystemProcessCommonFunctions::~ISystemProcessCommonFunctions() = default;

} // namespace Service::AM
