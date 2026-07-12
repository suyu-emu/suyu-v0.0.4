// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/service/applet_alternative_functions.h"

namespace Service::AM {

IAppletAlternativeFunctions::IAppletAlternativeFunctions(Core::System& system_)
    : ServiceFramework{system_, "IAppletAlternativeFunctions"} {}

IAppletAlternativeFunctions::~IAppletAlternativeFunctions() = default;

} // namespace Service::AM
