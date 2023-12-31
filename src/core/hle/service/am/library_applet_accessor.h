// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/am/frontend/applets.h"
#include "core/hle/service/service.h"

namespace Service::AM {

class ILibraryAppletAccessor final : public ServiceFramework<ILibraryAppletAccessor> {
public:
    explicit ILibraryAppletAccessor(Core::System& system_,
                                    std::shared_ptr<Frontend::FrontendApplet> applet_);

private:
    void GetAppletStateChangedEvent(HLERequestContext& ctx);
    void IsCompleted(HLERequestContext& ctx);
    void GetResult(HLERequestContext& ctx);
    void PresetLibraryAppletGpuTimeSliceZero(HLERequestContext& ctx);
    void Start(HLERequestContext& ctx);
    void RequestExit(HLERequestContext& ctx);
    void PushInData(HLERequestContext& ctx);
    void PopOutData(HLERequestContext& ctx);
    void PushInteractiveInData(HLERequestContext& ctx);
    void PopInteractiveOutData(HLERequestContext& ctx);
    void GetPopOutDataEvent(HLERequestContext& ctx);
    void GetPopInteractiveOutDataEvent(HLERequestContext& ctx);
    void GetIndirectLayerConsumerHandle(HLERequestContext& ctx);

    std::shared_ptr<Frontend::FrontendApplet> applet;
};

} // namespace Service::AM
