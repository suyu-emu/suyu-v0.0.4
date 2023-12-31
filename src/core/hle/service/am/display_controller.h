// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

class IDisplayController final : public ServiceFramework<IDisplayController> {
public:
    explicit IDisplayController(Core::System& system_);
    ~IDisplayController() override;

private:
    void GetCallerAppletCaptureImageEx(HLERequestContext& ctx);
    void TakeScreenShotOfOwnLayer(HLERequestContext& ctx);
    void AcquireLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void AcquireCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
};

} // namespace Service::AM
