// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct Applet;

class IDisplayController final : public ServiceFramework<IDisplayController> {
public:
    explicit IDisplayController(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~IDisplayController() override;

private:
    void GetCallerAppletCaptureImageEx(HLERequestContext& ctx);
    void TakeScreenShotOfOwnLayer(HLERequestContext& ctx);
    void AcquireLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseLastForegroundCaptureSharedBuffer(HLERequestContext& ctx);
    void AcquireCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseCallerAppletCaptureSharedBuffer(HLERequestContext& ctx);
    void AcquireLastApplicationCaptureSharedBuffer(HLERequestContext& ctx);
    void ReleaseLastApplicationCaptureSharedBuffer(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
