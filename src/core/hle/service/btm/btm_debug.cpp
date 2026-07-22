// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/hle/service/btm/btm_debug.h"

namespace Service::BTM {

IBtmDebug::IBtmDebug(Core::System& system_) : ServiceFramework{system_, "btm:dbg"} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "AcquireDiscoveryEvent"},
        {1, nullptr, "StartDiscovery"},
        {2, nullptr, "CancelDiscovery"},
        {3, nullptr, "GetDeviceProperty"},
        {4, nullptr, "CreateBond"},
        {5, nullptr, "CancelBond"},
        {6, nullptr, "SetTsiMode"},
        {7, nullptr, "GeneralTest"},
        {8, nullptr, "HidConnect"},
        {9, nullptr, "GeneralGet"}, //5.0.0+
        {10, nullptr, "GetGattClientDisconnectionReason"}, //5.0.0+
        {11, nullptr, "GetBleConnectionParameter"}, //5.1.0+
        {12, nullptr, "GetBleConnectionParameterRequest"}, //5.1.0+
        {13, nullptr, "GetDiscoveredDevice"}, //12.0.0+
        {14, nullptr, "SleepAwakeLoopTest"}, //15.0.0+
        {15, nullptr, "SleepTest"}, //15.0.0+
        {16, nullptr, "MinimumAwakeTest"}, //15.0.0+
        {17, nullptr, "ForceEnableBtm"}, //15.0.0+
    };
    // clang-format on

    RegisterHandlers(functions);
}

IBtmDebug::~IBtmDebug() = default;

} // namespace Service::BTM
