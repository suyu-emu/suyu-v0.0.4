// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "core/hle/service/service.h"

namespace Service::AM {

struct AppletStorageHolder;
struct Applet;

class ILibraryAppletAccessor final : public ServiceFramework<ILibraryAppletAccessor> {
public:
    explicit ILibraryAppletAccessor(Core::System& system_,
                                    std::shared_ptr<AppletStorageHolder> storage_,
                                    std::shared_ptr<Applet> applet_);
    ~ILibraryAppletAccessor();

protected:
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

    const std::shared_ptr<AppletStorageHolder> storage;
    const std::shared_ptr<Applet> applet;
};

} // namespace Service::AM
