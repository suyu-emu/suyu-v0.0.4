// SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {
    struct Applet;

    class IOverlayFunctions final : public ServiceFramework<IOverlayFunctions> {
    public:
        explicit IOverlayFunctions(Core::System &system_, std::shared_ptr<Applet> applet);
        ~IOverlayFunctions() override;

    private:
        Result BeginToWatchShortHomeButtonMessage();
        Result EndToWatchShortHomeButtonMessage();
        Result GetApplicationIdForLogo(Out<u64> out_application_id);
        Result SetAutoSleepTimeAndDimmingTimeEnabled(bool enabled);
        Result IsHealthWarningRequired(Out<bool> is_required);
        Result SetHandlingHomeButtonShortPressedEnabled(bool enabled);
        Result Unknown70();

    private:
        const std::shared_ptr<Applet> m_applet;
    };
} // namespace Service::AM
