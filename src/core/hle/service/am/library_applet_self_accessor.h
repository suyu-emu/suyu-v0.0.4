// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <vector>

#include "core/hle/service/service.h"

namespace Service::AM {

struct AppletStorageHolder;
struct Applet;

class ILibraryAppletSelfAccessor final : public ServiceFramework<ILibraryAppletSelfAccessor> {
public:
    explicit ILibraryAppletSelfAccessor(Core::System& system_, std::shared_ptr<Applet> applet_);
    ~ILibraryAppletSelfAccessor() override;

private:
    void PopInData(HLERequestContext& ctx);
    void PushOutData(HLERequestContext& ctx);
    void PopInteractiveInData(HLERequestContext& ctx);
    void PushInteractiveOutData(HLERequestContext& ctx);
    void GetPopInDataEvent(HLERequestContext& ctx);
    void GetPopInteractiveInDataEvent(HLERequestContext& ctx);
    void GetLibraryAppletInfo(HLERequestContext& ctx);
    void GetMainAppletIdentityInfo(HLERequestContext& ctx);
    void ExitProcessAndReturn(HLERequestContext& ctx);
    void GetCallerAppletIdentityInfo(HLERequestContext& ctx);
    void GetDesirableKeyboardLayout(HLERequestContext& ctx);
    void GetMainAppletAvailableUsers(HLERequestContext& ctx);
    void ShouldSetGpuTimeSliceManually(HLERequestContext& ctx);

    const std::shared_ptr<Applet> applet;
    const std::shared_ptr<AppletStorageHolder> storage;
};

} // namespace Service::AM
