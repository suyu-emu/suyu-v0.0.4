// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "core/hle/service/am/applet.h"
#include "core/hle/service/am/applet_common_functions.h"
#include "core/hle/service/ipc_helpers.h"

namespace Service::AM {

IAppletCommonFunctions::IAppletCommonFunctions(Core::System& system_,
                                               std::shared_ptr<Applet> applet_)
    : ServiceFramework{system_, "IAppletCommonFunctions"}, applet{std::move(applet_)} {
    // clang-format off
    static const FunctionInfo functions[] = {
        {0, nullptr, "SetTerminateResult"},
        {10, nullptr, "ReadThemeStorage"},
        {11, nullptr, "WriteThemeStorage"},
        {20, nullptr, "PushToAppletBoundChannel"},
        {21, nullptr, "TryPopFromAppletBoundChannel"},
        {40, nullptr, "GetDisplayLogicalResolution"},
        {42, nullptr, "SetDisplayMagnification"},
        {50, nullptr, "SetHomeButtonDoubleClickEnabled"},
        {51, nullptr, "GetHomeButtonDoubleClickEnabled"},
        {52, nullptr, "IsHomeButtonShortPressedBlocked"},
        {60, nullptr, "IsVrModeCurtainRequired"},
        {61, nullptr, "IsSleepRequiredByHighTemperature"},
        {62, nullptr, "IsSleepRequiredByLowBattery"},
        {70, &IAppletCommonFunctions::SetCpuBoostRequestPriority, "SetCpuBoostRequestPriority"},
        {80, nullptr, "SetHandlingCaptureButtonShortPressedMessageEnabledForApplet"},
        {81, nullptr, "SetHandlingCaptureButtonLongPressedMessageEnabledForApplet"},
        {90, nullptr, "OpenNamedChannelAsParent"},
        {91, nullptr, "OpenNamedChannelAsChild"},
        {100, nullptr, "SetApplicationCoreUsageMode"},
        {300, &IAppletCommonFunctions::GetCurrentApplicationId, "GetCurrentApplicationId"},
    };
    // clang-format on

    RegisterHandlers(functions);
}

IAppletCommonFunctions::~IAppletCommonFunctions() = default;

void IAppletCommonFunctions::SetCpuBoostRequestPriority(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::RequestParser rp{ctx};

    std::scoped_lock lk{applet->lock};
    applet->cpu_boost_request_priority = rp.Pop<s32>();

    IPC::ResponseBuilder rb{ctx, 2};
    rb.Push(ResultSuccess);
}

void IAppletCommonFunctions::GetCurrentApplicationId(HLERequestContext& ctx) {
    LOG_WARNING(Service_AM, "(STUBBED) called");

    IPC::ResponseBuilder rb{ctx, 4};
    rb.Push(ResultSuccess);
    rb.Push<u64>(system.GetApplicationProcessProgramID() & ~0xFFFULL);
}

} // namespace Service::AM
