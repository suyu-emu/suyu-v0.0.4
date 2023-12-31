// SPDX-FileCopyrightText: Copyright 2024 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <deque>
#include <vector>

#include "core/hle/service/service.h"

namespace Service::AM {

class ILibraryAppletSelfAccessor final : public ServiceFramework<ILibraryAppletSelfAccessor> {
public:
    explicit ILibraryAppletSelfAccessor(Core::System& system_);
    ~ILibraryAppletSelfAccessor() override;

private:
    void PopInData(HLERequestContext& ctx);
    void PushOutData(HLERequestContext& ctx);
    void GetLibraryAppletInfo(HLERequestContext& ctx);
    void GetMainAppletIdentityInfo(HLERequestContext& ctx);
    void ExitProcessAndReturn(HLERequestContext& ctx);
    void GetCallerAppletIdentityInfo(HLERequestContext& ctx);
    void GetDesirableKeyboardLayout(HLERequestContext& ctx);
    void GetMainAppletAvailableUsers(HLERequestContext& ctx);
    void ShouldSetGpuTimeSliceManually(HLERequestContext& ctx);

    void PushInShowAlbum();
    void PushInShowCabinetData();
    void PushInShowMiiEditData();
    void PushInShowSoftwareKeyboard();
    void PushInShowController();

    std::deque<std::vector<u8>> queue_data;
};

} // namespace Service::AM
